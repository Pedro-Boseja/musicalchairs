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

constexpr int NUM_JOGADORES = 4;
std::unique_ptr<std::counting_semaphore<NUM_JOGADORES>> cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(NUM_JOGADORES - 1);

std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
std::mutex print_mutex;

class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {
        for (int i = 1; i <= num_jogadores; ++i)
            jogadores_ativos.push_back(i);
    }

    void iniciar_rodada() {
        std::unique_lock<std::mutex> lock(music_mutex);
        musica_parada = false;
        cadeira_ocupada.assign(cadeiras, false);
        std::cout << "\n----------------------------------------------------------\n";
        std::cout << "Rodada com " << jogadores_ativos.size() << " jogadores e " << cadeiras << " cadeiras.\n";
        std::cout << "Jogadores ativos: ";
        for (int id : jogadores_ativos)
            std::cout << "P" << id << " ";
        std::cout << "\nA música está tocando... 🎵\n";
    }

    void parar_musica() {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1500 + 500));
        {
            std::lock_guard<std::mutex> lock(music_mutex);
            musica_parada = true;
        }
        std::cout << "> A música parou! Os jogadores estão tentando se sentar...\n";
        music_cv.notify_all();
    }

    void eliminar_jogador_nao_sentado(const std::vector<int>& sentados) {
        std::vector<int> jogadores_para_eliminar;
    
        // Verificar quais jogadores não se sentaram
        for (int id : jogadores_ativos) {
            if (std::find(sentados.begin(), sentados.end(), id) == sentados.end()) {
                jogadores_para_eliminar.push_back(id);  // Adicionar jogadores a serem eliminados
            }
        }
    
        // Eliminar apenas um jogador por vez (o primeiro da lista)
        if (!jogadores_para_eliminar.empty()) {
            int jogador_eliminado = jogadores_para_eliminar.front(); // Eliminar o primeiro jogador da lista
            jogadores_ativos.erase(std::remove(jogadores_ativos.begin(), jogadores_ativos.end(), jogador_eliminado), jogadores_ativos.end());
            std::cout << "\nJogador P" << jogador_eliminado << " não conseguiu uma cadeira e foi eliminado!\n";
        }
    }    

    void reduzir_cadeiras() {
        if (cadeiras > 0) {
            cadeiras--;
            cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(cadeiras);
        }
    }

    bool jogo_continua() {
        return jogadores_ativos.size() > 1 && cadeiras > 0;
    }

    int get_cadeiras() const { return cadeiras; }
    const std::vector<int>& get_jogadores_ativos() const { return jogadores_ativos; }
    void marcar_ocupada(int idx) { cadeira_ocupada[idx] = true; }
    bool is_ocupada(int idx) const { return cadeira_ocupada[idx]; }

private:
    int num_jogadores;
    int cadeiras;
    std::vector<bool> cadeira_ocupada;
    std::vector<int> jogadores_ativos;

    friend class Jogador;
    friend class Coordenador;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false) {}

    void joga() {
    while (jogo_ativo && !eliminado) {
        if (std::find(jogo.get_jogadores_ativos().begin(), jogo.get_jogadores_ativos().end(), id) == jogo.get_jogadores_ativos().end()) {
            // Se o jogador foi eliminado, ele não deve continuar jogando
            break;
        }

        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, [] { return musica_parada.load() || !jogo_ativo.load(); });
        lock.unlock();

        if (!jogo_ativo || eliminado) break;

        if (jogo.get_cadeiras() > 0 && cadeira_sem->try_acquire()) {
            for (int i = 0; i < jogo.get_cadeiras(); ++i) {
                if (!jogo.is_ocupada(i)) {
                    jogo.marcar_ocupada(i);
                    cadeiras_sentadas.push_back(id);

                    {
                        std::lock_guard<std::mutex> print_lock(print_mutex); // Protege impressão
                        std::cout << "[Cadeira " << (i + 1) << "]: Ocupada por P" << id << "\n";
                    }

                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

    bool foi_eliminado() const { return eliminado; }
    void set_eliminado() { eliminado = true; }
    int get_id() const { return id; }
    static std::vector<int> cadeiras_sentadas;

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;
};

std::vector<int> Jogador::cadeiras_sentadas;

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        while (jogo.jogo_continua()) {
            jogo.iniciar_rodada();
            Jogador::cadeiras_sentadas.clear();

            jogo.parar_musica();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            jogo.eliminar_jogador_nao_sentado(Jogador::cadeiras_sentadas);
            jogo.reduzir_cadeiras();
        }

        jogo_ativo = false;
        musica_parada = true;
        music_cv.notify_all();

        if (!jogo.get_jogadores_ativos().empty())
            std::cout << "\n🏆 Vencedor: Jogador P" << jogo.get_jogadores_ativos().front() << "! Parabéns! 🏆\n";
    }

private:
    JogoDasCadeiras& jogo;
};

int main() {
    std::cout << "----------------------------------------------------------\n";
    std::cout << "        Bem-vindo ao Jogo das Cadeiras Concorrente!\n";
    std::cout << "----------------------------------------------------------\n";

    srand(static_cast<unsigned>(time(nullptr)));
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);

    std::vector<Jogador> jogadores;
    for (int i = 1; i <= NUM_JOGADORES; ++i)
        jogadores.emplace_back(i, jogo);

    std::vector<std::thread> threads;
    for (auto& j : jogadores)
        threads.emplace_back(&Jogador::joga, &j);

    std::thread t_coord(&Coordenador::iniciar_jogo, &coordenador);

    for (auto& t : threads)
        if (t.joinable()) t.join();

    if (t_coord.joinable())
        t_coord.join();

    std::cout << "\nJogo das Cadeiras finalizado.\n";

    std::cout << "\nObrigado por jogar o Jogo das Cadeiras Concorrente!\n\n";

    return 0;
}
//compilação: g++ -std=c++20 -pthread -o jogo main.cpp
//execução: ./jogo

