build: 
	g++ -o main uthreads.cpp uthreads.h uthreads_utils.cpp 
run: 
	./main
deploy:
	g++ -o main uthreads.cpp uthreads.h uthreads_utils.cpp 
	./main