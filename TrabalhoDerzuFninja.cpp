#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;

int main() {
    
    CascadeClassifier face_cascade;
    if (!face_cascade.load("haarcascade_frontalface_default.xml")) {
        cout << "Erro ao carregar o classificador de faces!" << endl;
        return -1;
    }

    // Abre a câmera
    VideoCapture cap("video.mp4");
    if (!cap.isOpened()) {
        cout << "Erro ao abrir a câmera!" << endl;
        return -1;
    }

    // mecanica do jogo
    int score = 0;
    vector<Point2f> fruits;
    RNG rng; // Gerador aleatório para as frutas
    int fruit_radius = 30; // Tamanho da fruta
    int max_fruits = 5; // Máximo de frutas na tela
    int fruit_speed = 5; // Velocidade das frutas
    Scalar fruit_color = Scalar(0, 255, 0); // Cor da fruta

    // Janela de visualização
    namedWindow("Fruit Ninja", WINDOW_AUTOSIZE);

    // Loop principal do jogo
    while (true) {
        cout << "entrei\n";
        Mat frame, gray_frame;
        cap >> frame;

        // Verifique se o frame foi capturado
        if (frame.empty()) break;

        cvtColor( frame, gray_frame, COLOR_BGR2GRAY );
        equalizeHist( gray_frame, gray_frame );

        // Detector de faces
        vector<Rect> faces;
        face_cascade.detectMultiScale( gray_frame, faces,
        1.3, 2, 0
        //|CASCADE_FIND_BIGGEST_OBJECT
        //|CASCADE_DO_ROUGH_SEARCH
        |CASCADE_SCALE_IMAGE,
        Size(40, 40) );


        // Detectar a maior face (caso haja mais de uma)
        if (!faces.empty()) {
            Rect largest_face = faces[0];
            for (size_t i = 1; i < faces.size(); i++) {
                if (faces[i].area() > largest_face.area()) {
                    largest_face = faces[i];
                }
            }

            // Desenhar a face detectada
            rectangle(frame, largest_face, Scalar(0, 0, 255), 2);

            // Adicionar frutas à tela
            if (fruits.size() < max_fruits) {
                int x = rng.uniform(50, frame.cols - 50);
                fruits.push_back(Point2f(x, 0));
            }

            // Movendo frutas e verificando colisões
            for (size_t i = 0; i < fruits.size(); i++) {
                fruits[i].y += fruit_speed; // Movendo para baixo

                // Verificando se a face "cortou" a fruta
                if (largest_face.contains(Point(fruits[i].x, fruits[i].y))) {
                    score += 10; // Adicionar pontos
                    fruits.erase(fruits.begin() + i); // Remover fruta cortada
                    i--; // Ajustar o índice
                }
                // Desenhar frutas
                else {
                    circle(frame, fruits[i], fruit_radius, fruit_color, -1);
                }
            }

            // Desenhar a pontuação na tela
            putText(frame, "Pontuacao: " + to_string(score), Point(10, 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(255, 255, 255), 2);
        }

        // Mostra a janela do jogo
        imshow("Fruit Ninja", frame);

        // Sai do jogo ao pressionar 'q'
        if (waitKey(30) == 'q') break;
    }

    // Liberar memória
    cap.release();
    destroyAllWindows();

    return 0;
}
