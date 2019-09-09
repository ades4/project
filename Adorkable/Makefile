CC=g++
bin=Adorkable 
src=Adorkable.cc    
INCLUDE=-Ispeech/
LIB=-ljsoncpp -lcurl -lcrypto -lpthread
FLAGS=-D_LOG_

$(bin):$(src)
	$(CC) -o $@ $^ $(LIB) $(FLAGS) -std=c++11 $(INCLUDE) 

.PHONY:clean
clean:
	rm -f $(bin)

