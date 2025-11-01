#include <iostream>
#include <cp_api/framework.hpp>

int main() {

    using namespace cp_api;
    try {
        cp_api::Framework framework;
        framework.Init();
        framework.Run();
    } catch(const std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}