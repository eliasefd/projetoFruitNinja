#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <SFML/Audio.hpp>
#include <fstream>
#include <algorithm>
#include <string>

using namespace cv;
using namespace std;
using namespace chrono;


// Estrutura para representar uma fruta ou bomba
struct Fruta {
    Point2f posicao;
    Mat imagem;
    int largura, altura;
    bool ehBomba; 
};

// Classe para gerenciar as pontuações
class GerenciadorDePontuacao {
private:
    string arquivo_pontuacao;

public:
    GerenciadorDePontuacao(const string& arquivo) : arquivo_pontuacao(arquivo) {}

    // Adiciona uma nova pontuação ao arquivo
    void adicionarPontuacao(int pontuacao) {
        ofstream ofs(arquivo_pontuacao, ios::app);
        if (ofs.is_open()) {
            ofs << pontuacao << endl;
            ofs.close();
        } else {
            cerr << "Não foi possível abrir o arquivo de pontuações para escrita." << endl;
        }
    }""

    // lê as pontuações salvas no arquivo
    vector<int> obterPontuacoes() {
        vector<int> pontuacoes;
        ifstream ifs(arquivo_pontuacao);
        if (ifs.is_open()) {
            string linha;
            while (getline(ifs, linha)) {
                try {
                    int pontuacao = stoi(linha);
                    pontuacoes.push_back(pontuacao);
                } catch (...) {
                    // Ignora linhas inválidas
                }
            }
            ifs.close();
        } else {
            cerr << "Não foi possível abrir o arquivo de pontuações para leitura." << endl;
        }
        return pontuacoes;
    }

    // Obtém as melhores pontuações em ordem decrescente
    vector<int> obterMelhoresPontuacoes(int top_n = 10) {
        vector<int> pontuacoes = obterPontuacoes();
        sort(pontuacoes.begin(), pontuacoes.end(), greater<int>());
        if (pontuacoes.size() > top_n)
            pontuacoes.resize(top_n);
        return pontuacoes;
    }
};

// Classe para exibir os menus
class Menu {
public:
    void mostrarMenu(Mat& frame, const string& titulo, const vector<string>& opcoes) {
        frame = Mat::zeros(480, 640, CV_8UC3);
        // Título
        putText(frame, titulo, Point(180, 100), FONT_HERSHEY_SIMPLEX, 1.5, Scalar(255, 255, 255), 2);
        // Opções
        int inicio_y = 200;
        int intervalo = 50;
        for (size_t i = 0; i < opcoes.size(); ++i) {
            putText(frame, to_string(i + 1) + ". " + opcoes[i], Point(150, inicio_y + i * intervalo), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
        }
    }
};

// Classe do jogo, sons, imagens
class FruitNinja {
private:
    VideoCapture cap;
    int pontuacao;
    int vidas;
    vector<Fruta> frutas;
    RNG rng;
    int velocidade_fruta;
    steady_clock::time_point tempo_inicio;
    int duracao_jogo;
    CascadeClassifier classificador_face;
    vector<Mat> imagens_frutas;
    Mat imagem_bomba;
    vector<pair<int, int>> tamanhos_frutas; // Largura e altura de cada fruta

    steady_clock::time_point ultimo_acrescimo_fruta;
    int intervalo_acrescimo_fruta;

    // Sons
    sf::SoundBuffer buffer_cortarFruta;
    sf::Sound som_cortarFruta;
    sf::SoundBuffer buffer_bomba;
    sf::Sound som_bomba;
    sf::SoundBuffer buffer_fimDeJogo;
    sf::Sound som_fimDeJogo;

public:
    FruitNinja(int duracao)
        : pontuacao(0), vidas(3), velocidade_fruta(5), duracao_jogo(duracao), intervalo_acrescimo_fruta(2000) {
        cap.open(0);
        if (!cap.isOpened()) {
            throw runtime_error("Erro ao abrir a câmera!");
        }

        if (!classificador_face.load("haarcascade_frontalface_default.xml")) {
            throw runtime_error("Erro ao carregar o classificador de face!");
        }

        // Carregar imagens de frutas
        vector<string> caminhos_frutas = {
            "resources/apple.png",
            "resources/pineapple.png",
            "resources/orange.png"
        };

        for (const auto& caminho : caminhos_frutas) {
            Mat img = imread(caminho, IMREAD_UNCHANGED);
            if (img.empty()) {
                cerr << "Erro ao carregar a imagem da fruta: " << caminho << endl;
                throw runtime_error("Falha no carregamento da imagem da fruta.");
            }

            // Verificar se a imagem tem 4 canais (incluindo alpha)
            if (img.channels() < 4) {
                cerr << "A imagem da fruta não possui canal alfa: " << caminho << endl;
                throw runtime_error("Imagem da fruta sem canal alfa.");
            }

            // Redimensionar a imagem da fruta, se necessário
            int largura_redimensionada = 60;
            int altura_redimensionada = 60;
            Mat img_redimensionada;
            resize(img, img_redimensionada, Size(largura_redimensionada, altura_redimensionada));

            imagens_frutas.push_back(img_redimensionada);
            tamanhos_frutas.emplace_back(largura_redimensionada, altura_redimensionada);
        }

        // Carregar imagem da bomba
        string caminho_bomba = "resources/bomb.png";
        imagem_bomba = imread(caminho_bomba, IMREAD_UNCHANGED);
        if (imagem_bomba.empty()) {
            cerr << "Erro ao carregar a imagem da bomba: " << caminho_bomba << endl;
            throw runtime_error("Falha no carregamento da imagem da bomba.");
        }

        // Verificar se a bomba tem canal alfa
        if (imagem_bomba.channels() < 4) {
            cerr << "A imagem da bomba não possui canal alfa: " << caminho_bomba << endl;
            throw runtime_error("Imagem da bomba sem canal alfa.");
        }

        // Redimensionar a imagem da bomba, se necessário
        int largura_bomba = 60;
        int altura_bomba = 60;
        resize(imagem_bomba, imagem_bomba, Size(largura_bomba, altura_bomba));

        // Sons
        if (!buffer_cortarFruta.loadFromFile("resources/sounds/slice.wav")) {
            cerr << "Erro ao carregar o som de corte de fruta." << endl;
            throw runtime_error("Falha no carregamento do som de corte de fruta.");
        }
        som_cortarFruta.setBuffer(buffer_cortarFruta);

        if (!buffer_bomba.loadFromFile("resources/sounds/bomb.wav")) {
            cerr << "Erro ao carregar o som de bomba." << endl;
            throw runtime_error("Falha no carregamento do som de bomba.");
        }
        som_bomba.setBuffer(buffer_bomba);

        if (!buffer_fimDeJogo.loadFromFile("resources/sounds/game_over.wav")) {
            cerr << "Erro ao carregar o som de fim de jogo." << endl;
            throw runtime_error("Falha no carregamento do som de fim de jogo.");
        }
        som_fimDeJogo.setBuffer(buffer_fimDeJogo);
    }

    // Função para sobrepor uma imagem com transparência
    void sobreporImagem(Mat& fundo, const Mat& frente, Point2f localizacao) {
        for (int y = 0; y < frente.rows; ++y) {
            int fundoY = static_cast<int>(localizacao.y) + y;
            if (fundoY >= fundo.rows) break;
            if (fundoY < 0) continue;

            for (int x = 0; x < frente.cols; ++x) {
                int fundoX = static_cast<int>(localizacao.x) + x;
                if (fundoX >= fundo.cols) break;
                if (fundoX < 0) continue;

                Vec4b pixel_frente = frente.at<Vec4b>(y, x);
                Vec3b pixel_fundo = fundo.at<Vec3b>(fundoY, fundoX);

                double alfa = pixel_frente[3] / 255.0;
                for (int c = 0; c < 3; ++c) {
                    fundo.at<Vec3b>(fundoY, fundoX)[c] = pixel_fundo[c] * (1.0 - alfa) + pixel_frente[c] * alfa;
                }
            }
        }
    }

    // Função principal do jogo, retorna a pontuação ao final
    int iniciarJogo() {
        Mat frame;
        namedWindow("Fruit Ninja", WINDOW_AUTOSIZE);
        tempo_inicio = steady_clock::now();
        ultimo_acrescimo_fruta = steady_clock::now();

        while (true) {
            cap >> frame;
            flip(frame, frame, 1);

            if (frame.empty()) break;

            Mat frame_cinza;
            cvtColor(frame, frame_cinza, COLOR_BGR2GRAY);
            equalizeHist(frame_cinza, frame_cinza);

            vector<Rect> faces;
            classificador_face.detectMultiScale(frame_cinza, faces, 1.1, 3, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));  // Detecta faces

            // Adiciona frutas ou bombas aleatoriamente à tela
            auto tempo_atual = steady_clock::now();
            int desde_ultima_fruta = duration_cast<milliseconds>(tempo_atual - ultimo_acrescimo_fruta).count();
            if (frutas.size() < 5 && desde_ultima_fruta >= intervalo_acrescimo_fruta) {
                int x = rng.uniform(50, frame.cols - 50);
                int tipo_fruta = rng.uniform(0, 4);  // Três frutas (0-2) e uma bomba (3)

                Fruta nova_fruta;
                nova_fruta.posicao = Point2f(x, 0);

                if (tipo_fruta < 3) {
                    nova_fruta.imagem = imagens_frutas[tipo_fruta];
                    nova_fruta.largura = tamanhos_frutas[tipo_fruta].first;
                    nova_fruta.altura = tamanhos_frutas[tipo_fruta].second;
                    nova_fruta.ehBomba = false;
                }
                else {
                    nova_fruta.imagem = imagem_bomba;
                    nova_fruta.largura = 60;
                    nova_fruta.altura = 60;
                    nova_fruta.ehBomba = true;
                }

                frutas.push_back(nova_fruta);
                ultimo_acrescimo_fruta = tempo_atual; // Atualiza o tempo da última adição
            }

            // Move as frutas e verifica colisões
            for (size_t i = 0; i < frutas.size(); ) {
                frutas[i].posicao.y += velocidade_fruta;

                // Verifica se a fruta saiu da tela
                if (frutas[i].posicao.y > frame.rows) {
                    frutas.erase(frutas.begin() + i);
                }
                else {
                    // Certificar que a ROI está dentro do frame
                    int x = static_cast<int>(frutas[i].posicao.x);
                    int y = static_cast<int>(frutas[i].posicao.y);
                    int w = frutas[i].largura;
                    int h = frutas[i].altura;

                    // Ajusta x e y para que a ROI não ultrapasse os limites
                    if (x + w > frame.cols) w = frame.cols - x;
                    if (y + h > frame.rows) h = frame.rows - y;
                    if (x < 0) { w += x; x = 0; }
                    if (y < 0) { h += y; y = 0; }

                    // Verifica se w e h são positivos
                    if (w <= 0 || h <= 0) {
                        frutas.erase(frutas.begin() + i);
                        continue;
                    }

                    Rect rect_roi(x, y, w, h);
                    Mat roi = frame(rect_roi);
                    Mat fruta_redimensionada;
                    resize(frutas[i].imagem, fruta_redimensionada, Size(w, h));

                    // Use a função sobreporImagem para lidar com transparência
                    sobreporImagem(frame, fruta_redimensionada, Point2f(x, y));
                    i++;
                }
            }

            // Verifica se o rosto do jogador colidiu com uma fruta ou bomba
            for (const Rect& face : faces) {
                for (size_t i = 0; i < frutas.size(); ) {
                    // Calcula o centro da fruta
                    Point2f centro_fruta = Point2f(frutas[i].posicao.x + frutas[i].largura / 2.0,
                                                  frutas[i].posicao.y + frutas[i].altura / 2.0);

                    if (face.contains(centro_fruta)) {
                        if (frutas[i].ehBomba) {
                            vidas--;
                            som_bomba.play(); // Toca o som de bomba
                            // Feedback visual para a bomba capturada
                            circle(frame, centro_fruta, frutas[i].largura / 2, Scalar(0, 0, 255), 2);
                            if (vidas == 0) {
                                som_fimDeJogo.play(); // Toca o som de fim de jogo
                                putText(frame, "Fim de Jogo!", Point(220, 240), FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 2);
                                imshow("Fruit Ninja", frame);
                                waitKey(3000);
                                return pontuacao;
                            }
                        }
                        else {
                            pontuacao++;
                            som_cortarFruta.play(); // Toca o som de corte de fruta
                            // Feedback visual para a fruta cortada
                            circle(frame, centro_fruta, frutas[i].largura / 2, Scalar(0, 255, 0), 2);
                        }
                        frutas.erase(frutas.begin() + i);
                        // Não incrementa i, pois a fruta foi removida
                    }
                    else {
                        i++;
                    }
                }
            }

            // Desenha a pontuação, vidas e tempo na tela
            putText(frame, "Pontuação: " + to_string(pontuacao), Point(10, 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
            putText(frame, "Vidas: " + to_string(vidas), Point(10, 70), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);

            // Calcula o tempo restante
            auto tempo_decorrido = steady_clock::now() - tempo_inicio;
            int segundos_restantes = duracao_jogo - duration_cast<seconds>(tempo_decorrido).count();
            putText(frame, "Tempo: " + to_string(segundos_restantes) + "s", Point(10, 110), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);

            // Finaliza o jogo se o tempo acabar
            if (segundos_restantes <= 0) {
                som_fimDeJogo.play(); // Toca o som de fim de jogo
                putText(frame, "Fim de Jogo!", Point(220, 240), FONT_HERSHEY_SIMPLEX, 2, Scalar(0, 0, 255), 2);
                imshow("Fruit Ninja", frame);
                waitKey(3000);
                return pontuacao;
            }

            imshow("Fruit Ninja", frame);

            // Encerra o jogo se 'q' for pressionado
            if (waitKey(30) == 'q') break;
        }

        cap.release();
        destroyAllWindows();
        return pontuacao;
    }
};

// Função para exibir a tabela de recordes
void mostrarRecordes(Mat& frame, const vector<int>& melhores_pontuacoes) {
    frame = Mat::zeros(480, 640, CV_8UC3);
    putText(frame, "Recordes", Point(150, 50), FONT_HERSHEY_SIMPLEX, 1.5, Scalar(255, 255, 255), 2);

    int inicio_y = 100;
    int intervalo = 40;
    for (size_t i = 0; i < melhores_pontuacoes.size(); ++i) {
        string texto_pontuacao = to_string(i + 1) + ". " + to_string(melhores_pontuacoes[i]);
        putText(frame, texto_pontuacao, Point(250, inicio_y + i * intervalo), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
    }

    putText(frame, "Pressione 'e' para encerrar", Point(180, 400), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
}

int main() {
    try {
        Menu menu;
        Mat frame;
        GerenciadorDePontuacao gerenciador_pontuacao("pontuacoes.txt");
        bool encerrar_programa = false;

        while (!encerrar_programa) {
            // Mostrar menu principal
            menu.mostrarMenu(frame, "Fruit Ninja", { "Iniciar Jogo", "Sair" });
            imshow("Fruit Ninja", frame);

            char tecla;
            while (true) {
                tecla = waitKey(30);
                if (tecla == '1' || tecla == 's') { // Iniciar jogo
                    break;
                }
                else if (tecla == '2' || tecla == 'q') { // Sair
                    encerrar_programa = true;
                    break;
                }
            }

            if (encerrar_programa) {
                break;
            }

            // Iniciar o jogo
            FruitNinja jogo(90); // Duração de 90 segundos
            int pontuacao_final = jogo.iniciarJogo();

            // Salvar a pontuação
            gerenciador_pontuacao.adicionarPontuacao(pontuacao_final);

            // Mostrar menu de fim de jogo
            bool menu_fim_de_jogo = true;
            while (menu_fim_de_jogo) {
                menu.mostrarMenu(frame, "Fim de Jogo", { "Tentar Novamente", "Encerrar" });
                imshow("Fruit Ninja", frame);

                char tecla_fim;
                while (true) {
                    tecla_fim = waitKey(30);
                    if (tecla_fim == '1') { // Tentar novamente
                        menu_fim_de_jogo = false;
                        break;
                    }
                    else if (tecla_fim == '2') { // Encerrar
                        // Mostrar tabela de recordes
                        vector<int> melhores_pontuacoes = gerenciador_pontuacao.obterMelhoresPontuacoes();
                        mostrarRecordes(frame, melhores_pontuacoes);
                        imshow("Fruit Ninja", frame);

                        // Aguardar o usuário pressionar 'e' para encerrar
                        while (true) {
                            char tecla_encerrar = waitKey(30);
                            if (tecla_encerrar == 'e') {
                                menu_fim_de_jogo = false;
                                encerrar_programa = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        // Se o programa estiver para encerrar, liberar recursos
        destroyAllWindows();
    }
    catch (const exception& e) {
        cerr << e.what() << endl;
    }

    return 0;
}
