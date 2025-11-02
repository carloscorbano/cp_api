// #include <iostream>
// #include <cp_api/framework.hpp>
// #include <cp_api/core/serializable.hpp>

// int main() {

//     using namespace cp_api;
//     try {
//         cp_api::Framework framework;
//         framework.Init();
//         framework.Run();
//     } catch(const std::exception& e){
//         std::cerr << "Exception: " << e.what() << std::endl;
//         return -1;
//     }

//     return 0;
// }

#include "cp_api/core/serializable.hpp"
#include <iostream>

struct Stats : cp_api::SerializableBase {
    int hp = 0;
    int mp = 0;

    Stats() = default;
    Stats(int h, int m) : hp(h), mp(m) {}

    void RegisterFields() {
        RegisterField("hp", hp);
        RegisterField("mp", mp);
    }
};

struct Player : cp_api::SerializableBase {
    std::string name;
    int level = 1;
    std::vector<Stats> stats_history;
    std::unordered_map<std::string, Stats> current_stats;
    std::optional<Stats> optional_stats;
    std::unique_ptr<Stats> ptr_stats;

    void RegisterFields() {
        RegisterField("name", name);
        RegisterField("level", level);
        RegisterField("stats_history", stats_history);
        RegisterField("current_stats", current_stats);
        RegisterField("optional_stats", optional_stats);
        RegisterField("ptr_stats", ptr_stats);
    }
};

int main() {
    Player p;
    p.name = "Carlos";
    p.level = 10;

    // Preenchendo vetor
    Stats s1(100, 50);
    Stats s2(120, 60);
    s1.RegisterFields();
    s2.RegisterFields();
    p.stats_history.push_back(s1);
    p.stats_history.push_back(s2);

    // Preenchendo unordered_map
    Stats s3(80, 40);
    s3.RegisterFields();
    p.current_stats["base"] = s3;

    // optional
    Stats s4(90, 45);
    s4.RegisterFields();
    p.optional_stats = s4;

    // unique_ptr
    p.ptr_stats = std::make_unique<Stats>(Stats(200, 100));
    p.ptr_stats->RegisterFields();

    // Registrando campos do player
    p.RegisterFields();

    // Serializando
    nlohmann::json j = p.Serialize();
    std::cout << "JSON:\n" << j.dump(4) << "\n";

    // Serializando para BSON
    auto bson_data = p.SerializeBSON();
    std::cout << "BSON size: " << bson_data.size() << " bytes\n";

    // Desserializando
    Player p2;
    p2.RegisterFields();
    p2.Deserialize(j);
    std::cout << "Player name after deserialization: " << p2.name << "\n";
    std::cout << "Level: " << p2.level << "\n";
    std::cout << "Stats history size: " << p2.stats_history.size() << "\n";
    if (p2.optional_stats.has_value()) {
        std::cout << "Optional HP: " << p2.optional_stats->hp << "\n";
    }
    if (p2.ptr_stats) {
        std::cout << "Ptr Stats HP: " << p2.ptr_stats->hp << "\n";
    }

    return 0;
}
