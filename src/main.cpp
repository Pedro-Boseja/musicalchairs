#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <set>
#include <memory>

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::unique_ptr<std::counting_semaphore<NUM_JOGADORES>> cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
std::mutex cadeiras_mutex;
std::vector<std::pair<int, int>> cadeiras_ocupadas;

// Classes
class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {
        for (int i = 1; i <= num_jogadores; ++i) {
            jogadores_ativos.push_back(i);
        }
    }

    void iniciar_rodada() {
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada = false;
        cadeira_ocupada_flags.clear();
        cadeiras_ocupadas.clear();
        cadeira_ocupada_flags.resize(cadeiras, false);
        exibir_estado();
        std::cout << "A música está tocando... \U0001F3B5" << std::endl;
    }

    void parar_musica() {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1500 + 500));
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada = true;
        std::cout << "> A música parou! Os jogadores estão tentando se sentar..." << std::endl;
        music_cv.notify_all();
    }

    bool eliminar_jogador(int jogador_id) {
        std::lock_guard<std::mutex> lock(music_mutex);
        auto it = std::find(jogadores_ativos.begin(), jogadores_ativos.end(), jogador_id);
        if (it != jogadores_ativos.end()) {
            jogadores_ativos.erase(it);
            std::cout << "\nJogador P" << jogador_id << " não conseguiu uma cadeira e foi eliminado!" << std::endl;
            return true;
        }
        return false;
    }

    int eliminar_nao_sentado() {
        std::set<int> jogadores_sentados;
        for (auto& par : cadeiras_ocupadas) {
            jogadores_sentados.insert(par.second);
        }

        for (int id : jogadores_ativos) {
            if (!jogadores_sentados.count(id)) {
                eliminar_jogador(id);
                return id;  // Apenas um eliminado por rodada
            }
        }
        return -1; // Todos sentaram
    }

    void exibir_estado() {
        std::cout << "\n-----------------------------------------------\n";
        std::cout << "Rodada com " << jogadores_ativos.size() << " jogadores e " << cadeiras << " cadeiras." << std::endl;
        std::cout << "Jogadores ativos: ";
        for (int id : jogadores_ativos) {
            std::cout << "P" << id << " ";
        }
        std::cout << "\n-----------------------------------------------" << std::endl;
    }

    void reduzir_cadeiras() {
        if (cadeiras > 0) {
            --cadeiras; // Reduz o número de cadeiras após cada rodada
            cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(cadeiras); // Recria semáforo
        }
    }

    bool jogo_continua() {
        return jogadores_ativos.size() > 1 && cadeiras > 0;
    }

    int get_num_jogadores() const { return num_jogadores; }
    int get_cadeiras() const { return cadeiras; }

    std::vector<int> jogadores_ativos;
    std::vector<bool> cadeira_ocupada_flags;

private:
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false) {}

    void tentar_ocupar_cadeira() {
        if (jogo.get_cadeiras() == 0) return;
        cadeira_sem->acquire();
        std::lock_guard<std::mutex> lock(cadeiras_mutex);

        for (size_t i = 0; i < jogo.cadeira_ocupada_flags.size(); ++i) {
            if (!jogo.cadeira_ocupada_flags[i]) {
                jogo.cadeira_ocupada_flags[i] = true;
                cadeiras_ocupadas.emplace_back(i + 1, id);
                std::cout << "[Cadeira " << (i + 1) << "]: Ocupada por P" << id << std::endl;
                return;
            }
        }
    }

    void verificar_eliminacao() {
        if (!eliminado && std::find(jogo.jogadores_ativos.begin(), jogo.jogadores_ativos.end(), id) == jogo.jogadores_ativos.end()) {
            eliminado = true;
        }
    }

    void joga() {
        while (jogo_ativo && !eliminado) {
            {
                std::unique_lock<std::mutex> lock(music_mutex);
                music_cv.wait(lock, [] { return musica_parada.load(); });
            }
            tentar_ocupar_cadeira();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            verificar_eliminacao();
        }
    }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        while (jogo.jogo_continua()) {
            jogo.iniciar_rodada();
            jogo.parar_musica();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            jogo.eliminar_nao_sentado();
            jogo.reduzir_cadeiras();
        }
        jogo_ativo = false;
        std::cout << "\n\U0001F3C6 Vencedor: Jogador P" << jogo.jogadores_ativos.front() << "! Parabéns! \U0001F3C6\n";
    }

private:
    JogoDasCadeiras& jogo;
};

// Main function
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> threads_jogadores;

    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (auto& jogador : jogadores_objs) {
        threads_jogadores.emplace_back(&Jogador::joga, &jogador);
    }

    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    for (auto& t : threads_jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}
