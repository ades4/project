#include "Adorkable.hpp"
using namespace std;

int main()
{
//	InterRobot r;
//	string str;
//	volatile bool quit = false;
//	while(!quit)
//	{
//		cout << "W # "; 
//		cin >> str ;
//		string s = r.Talk(str);
//		cout << "X $ "<< s << endl;
//
//	}
	Adorkable *wk = new Adorkable();
	if(!wk->LoadEt())
	{
		return 1;
	}
	wk->Run();
	return 0;
}

