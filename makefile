CXX=g++
CXXFLAGS=-Wall -g

LIB_SRC=uthreads.cpp uthreads.h
LIB_OBJ=$(LIB_SRC:.cpp=.o)

LIB=libuthreads.a

all: $(LIB)

$(LIB): $(LIB_OBJ)
	ar rcs $(LIB) $(LIB_OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LIB) $(LIB_OBJ)

save:
	git add -A 
	git commit -m "hello"
	git push