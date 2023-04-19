build: 
	g++ -o main uthreads.cpp uthreads.h uthreads_utils.cpp 
run: 
	./main
deploy:
	g++ -o main uthreads.cpp uthreads.h uthreads_utils.cpp 
	./main
save_git: 
	git add -A
	git commit -m "hello"
	git push