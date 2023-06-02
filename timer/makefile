CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

test: test_timer.cpp  tw_timer.cpp lst_timer.cpp ../http/http_conn.cpp ../log/log.cpp ../CGImysql/sql_connection_pool.cpp
	$(CXX) -o test  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r test