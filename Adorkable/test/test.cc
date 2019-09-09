#include <iostream>
#include <cstdio>
#include <string>
using namespace std;

int main()
{
	//string cmd = "ls -l -a";
	string cmd = "arecord -t wav -c 1 -r 16000 -d 5 -f S16_LE demo.wav";
	//popen()函数通过创建一个管道，调用fork()产生一个子进程，执行一个shell以运行命令来开启一个进程
	FILE *fp = popen(cmd.c_str(), "r");
	if(NULL == fp)
	{
		perror("popen");
		return 1;
	}
	char c;
	while(fread(&c, 1, 1, fp) > 0)    //读到C里面，读一个字符，读一个基本单位，从fp流里读
	{
		fwrite(&c, 1, 1, stdout);   //写到显示器上
	}


	pclose(fp);
	return 0;
}

