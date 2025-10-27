#include <iostream>
#include <cp_api/framework.hpp>
#include <cp_api/core/hybridEvents.hpp>

// Evento exemplo
struct ScoreEvent : cp_api::Event {
    std::string player;
    int points;

    ScoreEvent(const std::string& p, int pts) : player(p), points(pts) {}
};

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

    HybridEventDispatcher dispatcher;

    // Listener PRIORITY 3
    auto id1 = dispatcher.Subscribe<ScoreEvent>(
        Delegate<void(const ScoreEvent&)>::FromLambda([](const ScoreEvent& e){
            std::cout << "[Delegate Priority 3] " << e.player << " scored " << e.points << std::endl;
        }), 3
    );

    // Listener PRIORITY 1
    auto id2 = dispatcher.Subscribe<ScoreEvent>(
        Delegate<void(const ScoreEvent&)>::FromLambda([](const ScoreEvent& e){
            std::cout << "[Lambda Priority 1] " << e.player << " scored " << e.points << " points" << std::endl;
        }), 1
    );

    // Listener PRIORITY 5
    auto id3 = dispatcher.Subscribe<ScoreEvent>(
        Delegate<void(const ScoreEvent&)>::FromLambda([](const ScoreEvent& e){
            std::cout << "[Lambda Priority 5] Extra listener: " << e.player << " got " << e.points << " points" << std::endl;
        }), 5
    );

    // Inicia processamento assíncrono
    dispatcher.StartAsync();

    std::cout << "=== Enfileirando eventos assíncronos ===" << std::endl;

    // Enfileira múltiplos eventos
    dispatcher.QueueEvent(ScoreEvent("Alice", 42));
    dispatcher.QueueEvent(ScoreEvent("Bob", 99));
    dispatcher.QueueEvent(ScoreEvent("Charlie", 123));

    // Aguarda um pouco para a fila processar
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "=== Removendo listener PRIORITY 3 ===" << std::endl;
    dispatcher.Unsubscribe<ScoreEvent>(id1);

    dispatcher.QueueEvent(ScoreEvent("Daisy", 256));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Para o dispatcher assíncrono
    dispatcher.StopAsync();

    std::cout << "=== Fim do exemplo assíncrono ===" << std::endl;
    
    return 0;
}