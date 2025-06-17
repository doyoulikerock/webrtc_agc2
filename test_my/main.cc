#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <dlfcn.h>

using namespace std;

extern "C" int test_main(int, char **);

int main(int argc, char* argv[]){
	void *handle;
    int (*func)(int, char **);
    
    // .so 파일 로드
    handle = dlopen("liblibmy.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        return 1;
    }

    // 함수 주소 가져오기
    *(void **)(&func) = dlsym(handle, "test_main");
    if (!func) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

	


	//func(argc, argv);
	printf("test_main ...\n");
	test_main(argc, argv);
    return 0;
}
