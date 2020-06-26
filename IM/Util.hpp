#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <json/json.h>
#include "mongoose.h"
using namespace std;

class Util{
	public:
		static string mgStrToString(struct mg_str* str)
		{
			string msg = "";
			for(auto i = 0; i < str->len; i++)
			{
				msg.push_back(str->p[i]);
			}
			return msg;
		}
		static bool GetNameAndPasswd(string info, string &name, string &passwd){
			bool result;
			JSONCPP_STRING errs;
			Json::Value root;
			Json::CharReaderBuilder cb;
			std::unique_ptr<Json::CharReader> const cr(cb.newCharReader());
			result = cr->parse(info.data(), info.data()+info.size(), &root, &errs);
			if(!result || !errs.empty())
			{	
				cerr << "parse error" <<endl;
				return false;
			}

			name = root["name"].asString();
			passwd = root["passwd"].asString();
			return true;
		}
};
