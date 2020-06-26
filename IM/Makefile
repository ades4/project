cc=g++
bin=IMServer
src=IMServer.cc mongoose/mongoose.c 
include=-Imongoose/ -Imysql/include
lib=-Lmysql/lib    #链接的库在哪
lib_name=-lmysqlclient	-ljsoncpp	#链接的库是哪个
$(bin):$(src)
	$(cc) -o $@ $^ -std=c++11 $(include) $(lib) $(lib_name)
.PHONY:clean
clean:
	rm -f $(bin)
