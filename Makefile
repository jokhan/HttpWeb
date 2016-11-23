HPP=./include/
CXX=g++
LDFLAGS_COMMON=-std=c++11 -O3 -pthread -lboost_system
DEBUG=-ggdb3
SOURCE_HTTP=main_http.cpp
EXEC_HTTP=server_http

$(EXEC_HTTP): $(SOURCE_HTTP)
	$(CXX) $(SOURCE_HTTP) $(DEBUG) $(LDFLAGS_COMMON) -o $@
clean:
	rm -f *.o $(EXEC_HTTP)
