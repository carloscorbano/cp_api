# cp_api

Projeto-base em C++ para desenvolvimento de APIs/serviços com estrutura modular organizada em `api/` e `app/`.

## Tecnologias
- C++
- CMake
- vcpkg

## Estrutura
```
/api  → núcleo da API  
/app  → aplicação/execução  
CMakeLists.txt  
vcpkg.json
```

## Build
```bash
git clone https://github.com/carloscorbano/cp_api
cd cp_api
mkdir build && cd build
cmake ..
cmake --build .
```

## Uso
A aplicação/outputs serão gerados dentro de **/build**.  
Expanda a lógica dentro de `api/` e implemente execução em `app/`.

---

Contribuições são bem-vindas.  