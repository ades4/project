#pragma once

#include <iostream>
#include <cstdio>
#include <string>
#include <memory>
#include <sstream>
#include <map>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iterator>
#include <unistd.h>
#include <json/json.h>

#include "speech.h"
#include "base/http.h"

#define LOG "temp_file/log.txt"
#define ASR_PATH "temp_file/asr.wav"          //ASR语音识别的文件
#define TTS_PATH "temp_file/test.wav" 		  //TTS语音合成产生的文件
//#define TTS_PATH "temp_file/test.mp3" 		  //TTS语音合成产生的文件
#define CMD_ETC "temp_file/command.etc"       //命令配置文件

class Util
{
private:
	static pthread_t id;
public:
	static bool Exec(std::string command, bool is_print)
	{	

		if(!is_print)
		{
			command += ">/dev/null 2>&1";  //不想打印，把标准输出和标准错误都重定向到/null文件里
		}

		//popen()函数通过创建一个管道，调用fork()产生一个子进程，执行一个shell以运行命令来开启一个进程
		FILE *fp = popen(command.c_str(), "r");
		if(nullptr == fp)
		{
			std::cerr << "popen exec \'" << command << "\' Error" << std::endl;
			return false;
		}
		if(is_print)
		{
			char c;
			while(fread(&c, 1, 1, fp) > 0)    //读到C里面，读一个字符，读一个基本单位，从fp流里读
			{
				fwrite(&c, 1, 1, stdout);   //写到显示器上
			}

		}
		pclose(fp);
		return true;
	}
	static void* ThreadRun(void *arg)
	{
		const char *tips = (char*) arg;
		int i = 0;
		char bar[100];
		memset(bar, 0, sizeof(bar));
		const char *lable = "-/|\\";
		while(i <= 50)
		{
			printf("%s[%-50s][%d%%][%c]\r", tips, bar, i*2, lable[i%4]);
			fflush(stdout);
			bar[i++] = '=';
			bar[i] = '>';
			usleep(49000*2);
		}	
		printf("\n");
	}
	static void PrintfStart(std::string tips)
	{
		pthread_create(&id, NULL, ThreadRun, (void*)tips.c_str());
	}
	static void PrintfEnd()
	{
		pthread_cancel(id);
	}
};
pthread_t Util::id;
//访问百度语音识别类 
class SpeechRec
{	
private:
	std::string app_id;
	std::string api_key;
	std::string secret_key;
	aip::Speech *client;
private:
	bool IsCodeLegal(int code)
	{
		bool result = false;
		switch(code)
		{
		case 0:
			result = true;
			break;
		default:
			break;
		}
		return result;
	}
public:
	SpeechRec()
	{
		app_id = "16868252";
		api_key = "Vj6WXaOdl7RPU1UlebYKQpQX";
		secret_key = "cqkp1HWxnyOzB0fMUsbIxRyHVXrOdmzL";
		client = new aip::Speech(app_id, api_key, secret_key);   //语音识别的对象
	}
	//语音识别
	bool ASR(std::string path, std::string &out)
	{
		std::map<std::string, std::string> options;
		options["dev_pid"] = "1536";  //文字类型
		std::string file_content;   //字符串流
		aip::get_file_content(ASR_PATH, &file_content); //从文件里读到string流里
		Json::Value result = client->recognize(file_content, "wav", 16000, options); 
		int code = result["err_no"].asInt();
		if(!IsCodeLegal(code))
		{	
			std::cerr << "recognize error" <<std::endl;
			return false;
		}

		//识别成功
		out = result["result"][0].asString();
		return true;
	}
	//语音合成
	bool TTS(std::string message)
	{
		std::ofstream ofile;
		std::string file_ret;
		std::map<std::string, std::string> options;
		bool ret;
		options["spd"] = "5";  //语速
		options["per"] = "3";  //发音人选择
		options["aue"] = "6";
		// 合成成功的二进制数据写入文件中
	    ofile.open(TTS_PATH, std::ios::out | std::ios::binary);
		Json::Value result = client->text2audio(message, options, file_ret);
		
		// 如果file_ret为不为空则说明合成成功，返回文件内容
		if(!file_ret.empty()){
			ofile << file_ret; 
			ret = true;
		}
		else{
		    // 服务端合成错误
	        std::cout << "error: " << result.toStyledString();
			ret =  false;
	    }

		ofile.close();
		return ret;
	}
	~SpeechRec()
	{
	}

};
//图灵机器人交互式类 
class InterRobot
{
private:
	std::string url;
	//注册的图灵账号对应的apikey
	std::string api_key;
	//用户ID，可以随意指定
	std::string user_id;
	//使用百度语音识别speech自带的Http Client发起相关请求
	aip::HttpClient client;
private:
	bool IsCodeLegal(int code)
	{
		bool result = false;
		switch(code)
		{
		case 5000:
		case 6000:
			break;
		case 10004:
			result = true;
			break;
		default:
			result = true;
			break;
		}
		return result;
	}

	std::string MessageToJson(std::string &message) 
	{
		//序列化
		Json::Value root;
		Json::StreamWriterBuilder wb;
		std::ostringstream ss;		//字符串流的输出操作
		
		Json::Value item_;
		item_["text"] = message;

		Json::Value item;
		item["inputText"] = item_;

		root["reqType"] = 0;    //文本
		root["perception"] = item;

		item.clear();
		item["apiKey"] = api_key;
		item["userId"] = user_id;
		root["userInfo"] = item;
		
		std::unique_ptr<Json::StreamWriter> sw(wb.newStreamWriter());  //用一个智能指针调用writer方法
		sw->write(root, &ss);   //把Json串写进ss里
		std::string json_string = ss.str();
//		std::cout << "debug: " << json_string << std::endl;
		return json_string;
	}

	//发起http post请求 
	std::string RequestTL(std::string &request_json)
	{
		std::string response;
		int status_code = client.post(url, nullptr, request_json, nullptr, &response);
		if(status_code != CURLcode::CURLE_OK)   //连接失败
		{
			std::cerr << "post error" << std::endl;
			return "";
		}
            
        return response;
	}
		
	std::string JsonToEchoMessage(std::string &str)
	{
		//反序列化
		
		JSONCPP_STRING errs;   //错误
		Json::Value root;    //Json:Value理解为一个Json结构，这个类型可以嵌套，也可以当做数组一样使用，在需要的时候可以把这个结构转为相应的类型
		Json::CharReaderBuilder rb;		//可以读取标准输入流中的数据
		std::unique_ptr<Json::CharReader> const cr(rb.newCharReader());  //用一个智能指针将来调用read方法
		bool res = cr->parse(str.data(), str.data()+str.size(), &root, &errs);
		if(!res || !errs.empty())
		{
			std::cerr << "parse Error!" << std::endl;
			return "";
		}
		
		int code  = root["intent"]["code"].asInt();
		if(!IsCodeLegal(code))
		{
			std::cerr << "response code error" << std::endl;
			return "";
		}

		//code合法
		Json::Value item = root["results"][0];		
		std::string msg = item["values"]["text"].asString();
		return msg;
	}
public:	
	InterRobot(std::string id = "1")
	{
		this->url = "http://openapi.tuling123.com/openapi/api/v2";
		//注册的图灵账号对应的apikey
		this->api_key = "079924b244ae4ae880f315404d6ca28b";
		//用户ID，可以随意指定
		this->user_id = id;
	}


	//talk     1. 把message按照json格式转成对应的json串	2.经过http的post方法发出去请求图灵，得到响应后	3.解析响应字符串，提取响应massage，返回
	std::string Talk(std::string message)	
	{
		std::string json = MessageToJson(message);  //1. 把message按照json格式转成对应的json串
		std::string response = RequestTL(json);	//2.经过http的post方法,发出去请求图灵
		std::string echo_message =  JsonToEchoMessage(response);
	
		return echo_message;
	}


	~InterRobot()
	{
	}

};
//Adorkable 完成核心逻辑 
class Adorkable
{
private:
	InterRobot rt;
	SpeechRec sr;
	std::map<std::string, std::string> command_set;
private:
	bool Record()   //录音
	{
	    std::string tips =  "录音中,请讲话...";
		Util::PrintfStart(tips);

		std::string command = "arecord -t wav -c 1 -r 16000 -d 5 -f S16_LE ";
		command += ASR_PATH;
		bool ret =  Util::Exec(command, false);
		Util::PrintfEnd();

		std::cout << "识别中,请等待..." << std::endl;
		return ret;
	}
public:
	Adorkable()
	{
	}

	//加载命令执行配置文件
	bool LoadEt()
	{
		std::ifstream in(CMD_ETC);
		if(!in.is_open())
		{
			std::cerr << "open error" << std::endl;
			return false;
		}
		
		std::string sep = ":";
		char buffer[256];
		while(in.getline(buffer, sizeof(buffer)))
		{
			std::string str = buffer;
			std::size_t pos = str.find(sep);
			if(std::string::npos == pos)  //读到文件结束
			{
				std::cerr << "not find" << std::endl;
				continue;
			}

			std::string k = str.substr(0, pos);   //pos长度
			std::string v = str.substr(pos+sep.size());
			k+="。";		//后面识别的时候会带句号
			
			command_set.insert(std::make_pair(k, v));
			//command_set.insert({k, v});
		}
		std::cout << "Load command etc ... done" << std::endl;
		in.close();
		return true;
	}

	//判定消息是否是需要执行的命令
	bool MessageIsCommand(std::string message, std::string &cmd)
	{
		auto iter = command_set.find(message);
		if(iter == command_set.end())
		{
			return false;
		}

		cmd = iter->second;
		return true;
	}
	//使用百度语音合成接口，文本转语音，在使用cvlc进行本地播放
	bool TTSAndPlay(std::string message)
	{		
		std::string play = "cvlc --play-and-exit ";
		//播放完毕退出:--play-and-exit
		play += TTS_PATH;
		play += " >/dev/null 2>&1";
		sr.TTS(message);		//语音合成
		Util::Exec(play, false);		//执行播放
		return true;
	}

	//先录音
	void Run()
	{
		//如果定义了宏LOG就把错误消息重定向到log文件里，没有的话就不进行操作
#ifdef _LOG_		
		int fd = open(LOG, O_WRONLY|O_CREAT, 0644);

		dup2(fd, 2);		//输出重定向
#endif		
		volatile bool quit = false;
		while(!quit)
		{
			if(this->Record())
			{ 
				//开始语音识别
				std::string message;
				if(sr.ASR(ASR_PATH, message))
				{
					std::string cmd;
			//		LoadEt();
					std::cout << "我: " << message << std::endl;
					
					//1.commend 
					if(MessageIsCommand(message, cmd))
					{
						std::cout << "[Adorkable@localhonst]$ " << cmd << std::endl;
						Util::Exec(cmd, true);
						continue;
					}
					 //2.talking  
					if(message == "你走吧。")
					{
						std::string quit_msg = "那我走了，要记得想我哦";
						std::cout << "Adorkable# " << quit_msg <<std::endl;
						TTSAndPlay(quit_msg);
						exit(0);
					}
					else
					{
						std::string echo = rt.Talk(message);
						std::cout << "Adorkable# " << echo << std::endl;
						TTSAndPlay(echo);
					}

				}
				else
				{
					std::cerr << "Record error... " << std::endl;
				}
			}
		}
#ifdef _LOG_
		close(fd);
#endif
	}
	
	~Adorkable()
	{
	}

};

