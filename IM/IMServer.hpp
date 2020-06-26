#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include "Util.hpp"
#include "mongoose.h"
#include "mysql.h"

#define IM_DB "im_db"   //数据库
#define IM_PORT 3306	//数据库端口号

#define SESSION_TTL 600.0
#define SESSION_CHECK_INTERVAL 5.0
#define NUM 128		//必须是确定大小的数组，不能超过
#define SESSION_COOKIE_NAME "IM"
#define SESSION_COOKIE_USER "NAME"

struct mg_serve_http_opts s_http_server_opts;

struct session{
	uint64_t id;
	double created;
	double last_used;
	std::string name;
};

class Session{
	private:
		session sessions[NUM];
	public:
		Session()
		{
			for(auto i = 0; i < NUM; i++)
			{
				sessions[i].id = 0;
				sessions[i].name = "";
				sessions[i].created = 0.0;
				sessions[i].last_used = 0.0;

			}
		}
		bool IsLogin(http_message *hm)
		{
			return GetSession(hm);
		}

		bool GetSession(http_message *hm)
		{
			uint64_t sid;
			char ssid[64];
			char *ssid_p = ssid;
		
			struct mg_str *cookie_header = mg_get_http_header(hm, "cookie");
			if(nullptr == cookie_header)
			{
				return false;
			}
			if(!mg_http_parse_header2(cookie_header, SESSION_COOKIE_NAME, &ssid_p, sizeof(ssid))){
				return false;
			}

			//字符串转长整型，16进制
			////sid = strtoull(ssid, NULL, 16);
			
			sid = strtoull(ssid, NULL, 10);

			for(auto i = 0; i < NUM; i++)
			{
				if(sessions[i].id == sid)
				{
					sessions[i].last_used = mg_time();
					return true;
				}
			}
			return false;
		}

		bool CreateSession(string name, uint64_t &id)
		{
			int i = 0;
			for(; i < NUM; i++)		//找到没有储存信息的地方
			{
				if(sessions[i].id == 0){
					break;
				}
			}
			if(i == NUM)
			{
				return false;
			}

			sessions[i].created = sessions[i].last_used = mg_time();
			sessions[i].name = name;
			sessions[i].id = (uint64_t)(mg_time()*1000000L);//采用时间戳作为session ID
			
			id =  sessions[i].id;
			return true;
		}

		void DestroySession(struct session *s)
		{
			s->id = 0;   //只要将id设置为0，即代表该session失效
		}

		//定期检查session是否超时，超时的话，就移除该session
		void CheckSession()
		{
			//获取当前时间，减去session生命周期
			double threadhold = mg_time() - SESSION_TTL;
			for(auto i = 0; i < NUM; i++)
			{
				if(sessions[i].id > 0 && sessions[i].last_used < threadhold)
				{
					std::cerr << "Sessions: " << sessions[i].id <<" User: " << sessions[i].name << " idle long time ... close " << std::endl;
					DestroySession(sessions + i);
				}
			}
		}
		~Session()
		{
		}
};	

class MysqlClient{
	private:
		MYSQL *my;
		
  		bool ConnectMysql()   //连接数据库
  		{	
			mysql_set_character_set(my, "utf8");
  			if(!mysql_real_connect(my, "localhost", "root", "", IM_DB, IM_PORT, NULL, 0))
  			//host连接本地主机，user用户,密码，连接数据库名，端口号
  			{
  				std::cerr << "connect mysql error " << std::endl;
  				return false;
  			}			
  		//	mysql_query(my, "setnames 'utf8'");
  			std::cout << "connect mysql success " << std::endl;
  			return true;
		}
	public:
		MysqlClient(){
			my = mysql_init(NULL);    //初始化
		//	mysql_set_character_set(my, "utf8");
		}

  		bool InsertUser(std::string name, std::string passwd){
			ConnectMysql();
			std::string sql = "INSERT INTO user (name, passwd) values(\"";
			sql += name;
			sql += "\",\"";
			sql += passwd;
			sql += "\")";
			std::cout << sql << std::endl;
			int ret = mysql_query(my, sql.c_str());
			//std::cout << "ret: " << ret << std::endl;
			mysql_close(my);
			return ret = 0 ? true : false;
		}
		bool SelectUser(std::string name, std::string passwd){
			ConnectMysql();
			std::string sql = "SELECT * FROM user WHERE	name = \"";
			sql += name;
			sql += "\" AND passwd=\"";
			sql += passwd;
			sql += "\"";
			std::cout << sql << std::endl;
			int ret = mysql_query(my, sql.c_str());
			bool result = false;
			if(ret == 0){
				MYSQL_RES *res = mysql_store_result(my);  //如果mysql_query返回成功，那么我们就通过mysql_store_result 这个函数来读取结果,res变量主要用于保存查询的结果。同时该函数malloc了一片内存空间来存储查询过来的数据
				if(mysql_num_rows(res) > 0){  //获取结果行数
					std::cout <<"debug.....: " << mysql_num_rows(res) << std::endl;
					result = true;
   				}
				free(res); 
			}
			mysql_close(my);
			return result;
		}
		void InsertMessage();
		void SelectMessage();
		~MysqlClient()
		{
		}
};	

class IMServer{
	private:
		std::string port;  //服务器端口号
		struct mg_mgr mgr;  //mongoose 事件管理器   里面有一个链表的头部指针,管理所有的连接，之后可以通过遍历访问
		struct mg_connection *nc;  //listen socket 对应的connect
		volatile bool quit;  
		static MysqlClient mc;
		static Session sn;
	public:
		IMServer(std::string _port = "8080"):port(_port),quit(false)
		{
		}

		static int IsWebsocket(const struct mg_connection *nc)
		{
			return nc->flags & MG_F_IS_WEBSOCKET;     //检测链接是否是websocket的长链接	
		}

		static void Broadcast(struct mg_connection *nc, std::string msg)
		{
			struct mg_connection *c;
			for(c = mg_next(nc->mgr, NULL); \
				c != NULL; c = mg_next(nc->mgr, c)){
				//if(c == nc) continue;  //不给自己广播
				mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, msg.c_str(), msg.size());
			}
		}

		static void RegisterHandler(mg_connection *nc, int ev, void *data)
		{
			std::string code = "0";
			std::string echo_json = "{\"result\": ";
			struct http_message *hm = (struct http_message*)data;
			std::string method = Util::mgStrToString(&(hm->method));

			if(method == "POST"){
				std::string body = Util::mgStrToString(&hm->body);

				std::string name;
				std::string passwd;
				if(Util::GetNameAndPasswd(body, name, passwd) && !name.empty() && !passwd.empty())
				{
	                if(mc.InsertUser(name, passwd))
					{
						code = "0";
					}
					else
					{
						code = "1";
					}
				}
				else
				{
					code = "2";
				}
				echo_json += code;
				echo_json += "}";
				mg_printf(nc, "HTTP/1.1 200 OK\r\n\r\n");
				mg_printf(nc, "Content-Length: %lu\r\n\r\n", echo_json.size());
				mg_printf(nc, echo_json.data());
			}
			else
			{
				nc->flags |= MG_F_SEND_AND_CLOSE;   //相应完毕，完毕链接
			}
		}
		
		static void LoginHandler(mg_connection *nc, int ev, void *data)
		{
			if(ev == MG_EV_CLOSE)
			{	
				return;
			}
			std::string code = "0";
			std::string echo_json="{\"result\": ";
			std::string shead = "";
			struct http_message *hm = (struct http_message*)data;
			std::cout << "loginHandler ev: " << ev << std::endl;
			mg_printf(nc, "HTTP/1.1 200 OK\r\n");
			std::string method = Util::mgStrToString(&(hm->method));
			if(method == "POST")
			{   //比较两者的方法
				std::string name, passwd;
				std::string body = Util::mgStrToString(&(hm->body));
	//			std::cout << "login handler: " << body << std::endl;
				if(Util::GetNameAndPasswd(body, name, passwd) && !name.empty() && !passwd.empty())
				{
					if(mc.SelectUser(name, passwd))
					{
						uint64_t id = 0;
						if(sn.CreateSession(name, id))
						{
							stringstream ss;
							ss << "Set-Cookie: " << SESSION_COOKIE_NAME << "=" << id << "; path=/\r\n";
							ss << "Set-Cookie: " << SESSION_COOKIE_USER << "=" << name << "; path=/\r\n";
							std::string shead = ss.str();
		
							mg_printf(nc, shead.data());
							code = "0";
						}
						else
						{
							code = "3";
						}
					}
					else
					{	
						code = "1";
					}
				}
				else
				{
					code = "2";
				}
				echo_json += code;
				echo_json += "}";
				mg_printf(nc, "Content-Length: %lu\r\n\r\n", echo_json.size());
				mg_printf(nc, echo_json.data());
			}
			else
			{
				mg_serve_http(nc, hm, s_http_server_opts); //返回登录页面
			}
			nc->flags |= MG_F_SEND_AND_CLOSE; //相应完毕，完毕链接
		}

		//回调函数，谁调用谁填充参数
		static void EventHandler(mg_connection *nc, int ev, void *data)
		{	
			switch(ev){
				//正常的HTTP请求
				case MG_EV_HTTP_REQUEST:{
					struct http_message *hm = (struct http_message*)data;
					std::string uri = Util::mgStrToString(&hm->uri);
					if(uri.empty() || uri == "/" || uri == "/index.html"){
						if(sn.IsLogin(hm)){
				 	        mg_serve_http(nc, hm, s_http_server_opts);
						}
						else{
						    mg_http_send_redirect(nc, 302, mg_mk_str("/login.html"), mg_mk_str(NULL));
						}
					}
					else{
						mg_serve_http(nc, hm, s_http_server_opts);
					}
					nc->flags |= MG_F_SEND_AND_CLOSE;
					}	
					break;   //重定向完毕，退出事件处理逻辑，关闭链接
				//握手成功
				case MG_EV_WEBSOCKET_HANDSHAKE_DONE:{
						Broadcast(nc, "some body join...");
					}
					break;
				//先提取出内容，再广播给其他人
				case MG_EV_WEBSOCKET_FRAME:{
						struct websocket_message *wm = (struct websocket_message*)data;
						struct mg_str ms = {(const char*)wm->data, wm->size};
						std::string msg = Util::mgStrToString(&ms);
						Broadcast(nc, msg);
					}
					break;
				case MG_EV_CLOSE:
					if(IsWebsocket(nc)){
						Broadcast(nc, "Someone left...");
					}
					break;
				case MG_EV_TIMER:{
					sn.CheckSession();
					//能引起超时事件的只有listen绑定的链接 
					//这里就保证系统能定期检查是否有session超时
					mg_set_timer(nc, mg_time() + SESSION_CHECK_INTERVAL);
					}
					break;
				default:
					//std::cout << "other ev: " << ev << std::endl;
					break;
				}
		}		
		void InitServer()
		{
			signal(SIGPIPE, SIG_IGN);
			mg_mgr_init(&mgr, NULL);   //初始化事件管理器
			nc = mg_bind(&mgr, port.c_str(), EventHandler);  //绑定
			
			mg_register_http_endpoint(nc, "/LH", LoginHandler);//注册方法，当访问登录网页时，执行对应动作
			mg_register_http_endpoint(nc, "/RH", RegisterHandler);        //注册
			
			mg_set_protocol_http_websocket(nc);   //让服务器支持websocket
			s_http_server_opts.document_root = "web";  //设置属性
			mg_set_timer(nc, mg_time() + SESSION_CHECK_INTERVAL);
		}

		void Start()
		{
			int timeout = 10000;
			while(!quit){
				mg_mgr_poll(&mgr, timeout);  //监听所关心的事件 
		//		std::cout << "time out ..." << std::endl;
			}
		}

		~IMServer()
		{
			mg_mgr_free(&mgr);
		}
};
MysqlClient IMServer::mc;
//静态变量初始化
Session IMServer::sn;

