#clean为关键字，为伪目标，并不会生成临时文件
.PHONY : clean 

mutex_lock:
	g++ -g mutex_lock.cpp -o mutex_lock -lpthread

clean:
	rm -f mutex_lock
