#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;

const GLuint WIDTH = 800, HEIGHT = 600;

// =========================================================================
// VARIÁVEIS GLOBAIS DA CÂMERA E MOUSE
// =========================================================================
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

bool firstMouse = true;
float yaw   = -90.0f;	
float pitch =  0.0f;
float lastX =  WIDTH / 2.0;
float lastY =  HEIGHT / 2.0;

float deltaTime = 0.0f;	
float lastFrame = 0.0f;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f; 
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// =========================================================================
// ESTRUTURAS DE DADOS
// =========================================================================
struct Material {
    glm::vec3 Ka = glm::vec3(0.2f); 
    glm::vec3 Kd = glm::vec3(0.8f); 
    glm::vec3 Ks = glm::vec3(1.0f); 
    float Ns = 32.0f;               
};

struct Objeto3D {
    GLuint VAO = 0;
    int nVertices = 0;
    GLuint textureID = 0;
    Material material;
    
    glm::vec3 posicao = glm::vec3(0.0f);
    glm::vec3  rotacao = glm::vec3(0.0f);
    glm::vec3 escala = glm::vec3(1.0f);

    bool seguirTrajetoria = false; // Define se este objeto será animado pela curva
};

// =========================================================================
// CÓDIGO DOS SHADERS (PHONG)
// =========================================================================
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texCoord;
    layout (location = 2) in vec3 normal; 

    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(position, 1.0));
        Normal = mat3(transpose(inverse(model))) * normal;  
        TexCoord = texCoord;
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 color;

    in vec3 FragPos;
    in vec3 Normal;
    in vec2 TexCoord;

    uniform sampler2D ourTexture;
    uniform vec3 highlightColor;
    uniform vec3 viewPos; 

    uniform vec3 material_Ka;
    uniform vec3 material_Kd;
    uniform vec3 material_Ks;
    uniform float material_Ns;

    struct PointLight {
        vec3 position;
        vec3 color;
        bool enabled;
    };
    uniform PointLight lights[3];

    void main() {
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 result = vec3(0.0);
        
        for(int i = 0; i < 3; i++) {
            if (!lights[i].enabled) continue;

            // 1. Ambiente
            vec3 ambient = material_Ka * lights[i].color;

            // 2. Difusa
            vec3 lightDir = normalize(lights[i].position - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * material_Kd * lights[i].color;

            // 3. Especular
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_Ns);
            vec3 specular = spec * material_Ks * lights[i].color;

            // Atenuação
            float distance = length(lights[i].position - FragPos);
            float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

            ambient *= attenuation;
            diffuse *= attenuation;
            specular *= attenuation;

            result += (ambient + diffuse + specular);
        }

        vec4 texColor = texture(ourTexture, TexCoord);
        color = vec4(result, 1.0) * texColor * vec4(highlightColor, 1.0);
    }
)glsl";

// =========================================================================
// FUNÇÕES AUXILIARES E MATEMÁTICAS
// =========================================================================
string getDirectoryPath(const string& filePath) {
    size_t pos = filePath.find_last_of("\\/");
    return (std::string::npos == pos) ? "" : filePath.substr(0, pos + 1);
}

// EQUAÇÃO MATEMÁTICA DA CURVA DE BÉZIER CÚBICA (4 Pontos de Controle)
glm::vec3 calcularBezierCubica(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    glm::vec3 p = uuu * p0;               // (1-t)^3 * P0
    p += 3.0f * uu * t * p1;              // 3 * (1-t)^2 * t * P1
    p += 3.0f * u * tt * p2;              // 3 * (1-t) * t^2 * P2
    p += ttt * p3;                        // t^3 * P3

    return p;
}

int loadSimpleOBJ(string filePATH, int &nVertices, string &texturePath, Material &matOut)
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<GLfloat> vBuffer;

    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) return -1;

    string basePath = getDirectoryPath(filePATH);
    std::string line;

    while (std::getline(arqEntrada, line)) {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;

        if (word == "mtllib") {
            string mtlFile; ssline >> mtlFile;
            std::ifstream mtlEntrada((basePath + mtlFile).c_str());
            if (mtlEntrada.is_open()) {
                string mtlLine;
                while (std::getline(mtlEntrada, mtlLine)) {
                    std::istringstream mtlSS(mtlLine);
                    string mtlWord; mtlSS >> mtlWord;
                    if (mtlWord == "map_Kd") { mtlSS >> texturePath; texturePath = basePath + texturePath; }
                    else if (mtlWord == "Ka") mtlSS >> matOut.Ka.r >> matOut.Ka.g >> matOut.Ka.b;
                    else if (mtlWord == "Kd") mtlSS >> matOut.Kd.r >> matOut.Kd.g >> matOut.Kd.b;
                    else if (mtlWord == "Ks") mtlSS >> matOut.Ks.r >> matOut.Ks.g >> matOut.Ks.b;
                    else if (mtlWord == "Ns") mtlSS >> matOut.Ns;
                }
                mtlEntrada.close();
            }
        }
        else if (word == "v") { glm::vec3 v; ssline >> v.x >> v.y >> v.z; vertices.push_back(v); } 
        else if (word == "vt") { glm::vec2 vt; ssline >> vt.s >> vt.t; texCoords.push_back(vt); } 
        else if (word == "vn") { glm::vec3 vn; ssline >> vn.x >> vn.y >> vn.z; normals.push_back(vn); } 
        else if (word == "f") {
            while (ssline >> word) {
                int vi = 0, ti = 0, ni = 0;
                std::istringstream ss(word); std::string index;

                if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index, '/')) ti = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index)) ni = !index.empty() ? std::stoi(index) - 1 : 0;

                vBuffer.push_back(vertices[vi].x); vBuffer.push_back(vertices[vi].y); vBuffer.push_back(vertices[vi].z);
                
                if (!texCoords.empty()) { vBuffer.push_back(texCoords[ti].s); vBuffer.push_back(texCoords[ti].t); } 
                else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); }

                if (!normals.empty()) { vBuffer.push_back(normals[ni].x); vBuffer.push_back(normals[ni].y); vBuffer.push_back(normals[ni].z); } 
                else { vBuffer.push_back(0.0f); vBuffer.push_back(0.0f); vBuffer.push_back(1.0f); }
            }
        }
    }
    arqEntrada.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 8;  
    return VAO;
}

void carregarObjeto(Objeto3D& obj, std::string caminhoArquivo, glm::vec3 posInicial) {
    obj.posicao = posInicial;
    std::string texturePath;
    
    obj.VAO = loadSimpleOBJ(caminhoArquivo, obj.nVertices, texturePath, obj.material);

    if (obj.VAO == -1) { std::cout << "Erro ao carregar modelo: " << caminhoArquivo << std::endl; return; }

    if (!texturePath.empty()) {
        glGenTextures(1, &obj.textureID);
        glBindTexture(GL_TEXTURE_2D, obj.textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_set_flip_vertically_on_load(true);
        int width, height, nrChannels;
        unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        stbi_image_free(data);
    }
}

void carregarCenaDoArquivo(const std::string& caminhoArquivoCena, std::vector<Objeto3D>& cena) {
    std::ifstream arquivo(caminhoArquivoCena);
    if (!arquivo.is_open()) return;

    std::string linha;
    while (std::getline(arquivo, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        std::istringstream ss(linha);
        std::string caminhoModelo;
        float px, py, pz, sx, sy, sz, rx, ry, rz;

        if (ss >> caminhoModelo >> px >> py >> pz >> sx >> sy >> sz >> rx >> ry >> rz) {
            Objeto3D obj;
            carregarObjeto(obj, caminhoModelo, glm::vec3(px, py, pz));
            obj.escala = glm::vec3(sx, sy, sz);
            obj.rotacao = glm::vec3(rx, ry, rz);
            cena.push_back(obj);
        }
    }
    arquivo.close();
}

// =========================================================================
// FUNÇÃO MAIN
// =========================================================================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Visualizador Final", nullptr, nullptr);
    if (window == nullptr) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);

    // Compila os Shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL); glCompileShader(vertexShader);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL); glCompileShader(fragmentShader);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader); glDeleteShader(fragmentShader);

    // Configuração da Cena via arquivo de texto externa
    std::vector<Objeto3D> cena;
    carregarCenaDoArquivo("../assets/cena.txt", cena);

    // Configura o segundo objeto da cena (índice 1) para seguir a curva paramétrica
    if (cena.size() > 1) {
        cena[1].seguirTrajetoria = true;
    }

    // Variáveis de controle de seleção e transformações mecânicas
    int objSelecionado = 0;
    int modoTransformacao = 0; 
    bool tabPressionado = false;

    // --- VARIÁVEIS DA ANIMAÇÃO DE BÉZIER ---
    glm::vec3 p0(-4.0f, 2.0f, -2.0f);  // Ponto inicial
    glm::vec3 p1(-1.0f, 5.0f,  2.0f);  // Ponto de controle 1
    glm::vec3 p2( 2.0f, -3.0f, 2.0f);  // Ponto de controle 2
    glm::vec3 p3( 4.0f, 2.0f, -2.0f);  // Ponto final
    float tBezier = 0.0f;
    bool animacaoAtiva = false;
    bool spacePressionado = false;

    // Configurações e estados das 3 luzes (Iluminação de 3 pontos)
    glm::vec3 lightPositions[] = {
        glm::vec3(0.0f, 4.0f, 4.0f),   // Luz Principal (Key)
        glm::vec3(-4.0f, 2.0f, -2.0f),  // Luz de Preenchimento (Fill)
        glm::vec3(4.0f, 1.0f, -4.0f)   // Contra-luz (Backlight)
    };
    bool lightsEnabled[3] = {true, true, true};
    bool keyLuzPressionada[3] = {false, false, false};

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    // =========================================================================
    // LOOP PRINCIPAL
    // =========================================================================
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- CONTROLE DA CÂMERA (Setas do Teclado) ---
        float cameraSpeed = 5.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cameraPos += cameraSpeed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cameraPos -= cameraSpeed * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

        // --- CONTROLE DE PLAY/PAUSE DA ANIMAÇÃO (Barra de Espaço) ---
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!spacePressionado) {
                animacaoAtiva = !animacaoAtiva;
                spacePressionado = true;
                cout << "Animacao parametrica: " << (animacaoAtiva ? "PLAY" : "PAUSE") << endl;
            }
        } else { spacePressionado = false; }

        // --- ATUALIZAÇÃO DA CURVA DE BÉZIER ---
        if (animacaoAtiva) {
            tBezier += 0.25f * deltaTime; // Ajusta a velocidade do passo temporal t
            if (tBezier > 1.0f) tBezier = 0.0f; // Loop contínuo ao fim da curva
            
            for (auto& obj : cena) {
                if (obj.seguirTrajetoria) {
                    // Substitui a posição estática pela calculada na curva paramétrica
                    obj.posicao = calcularBezierCubica(p0, p1, p2, p3, tBezier);
                }
            }
        }

        // --- SELEÇÃO DE OBJETO (TAB) ---
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            if (!tabPressionado && cena.size() > 0) { objSelecionado = (objSelecionado + 1) % cena.size(); tabPressionado = true; }
        } else { tabPressionado = false; }

        // --- MODOS DE TRANSFORMAÇÃO (1, 2, 3) ---
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) modoTransformacao = 0; 
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) modoTransformacao = 1; 
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) modoTransformacao = 2; 

        // --- MANIPULAÇÃO MANUAL DO OBJETO SELECIONADO (WASD) ---
        if (cena.size() > 0) {
            Objeto3D& obj = cena[objSelecionado];
            float velObj = 0.05f;
            // Só permite mover manualmente se o objeto NÃO estiver preso seguindo a animação automática
            if (modoTransformacao == 0 && !obj.seguirTrajetoria) {
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.posicao.y += velObj;
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.posicao.y -= velObj;
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.posicao.x += velObj;
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.posicao.x -= velObj;
            } else if (modoTransformacao == 1) {
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.rotacao.x -= velObj;
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.rotacao.x += velObj;
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) obj.rotacao.y += velObj;
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) obj.rotacao.y -= velObj;
            } else if (modoTransformacao == 2) {
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) obj.escala += glm::vec3(velObj);
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) obj.escala -= glm::vec3(velObj);
            }
        }

        // --- CONTROLE DAS LUZES (TECLAS 7, 8, 9) ---
        if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
            if (!keyLuzPressionada[0]) { lightsEnabled[0] = !lightsEnabled[0]; keyLuzPressionada[0] = true; }
        } else keyLuzPressionada[0] = false;

        if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
            if (!keyLuzPressionada[1]) { lightsEnabled[1] = !lightsEnabled[1]; keyLuzPressionada[1] = true; }
        } else keyLuzPressionada[1] = false;

        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
            if (!keyLuzPressionada[2]) { lightsEnabled[2] = !lightsEnabled[2]; keyLuzPressionada[2] = true; }
        } else keyLuzPressionada[2] = false;

        // --- RENDERIZAÇÃO ---
        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Envia dados das 3 luzes
        for(int i = 0; i < 3; i++) {
            string prefix = "lights[" + to_string(i) + "].";
            glUniform3fv(glGetUniformLocation(shaderProgram, (prefix + "position").c_str()), 1, glm::value_ptr(lightPositions[i]));
            glUniform3f(glGetUniformLocation(shaderProgram, (prefix + "color").c_str()), 1.0f, 1.0f, 1.0f); 
            glUniform1i(glGetUniformLocation(shaderProgram, (prefix + "enabled").c_str()), lightsEnabled[i]);
        }

        // Desenha os objetos instanciados da cena
        for (int i = 0; i < cena.size(); i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cena[i].posicao);
            model = glm::rotate(model, cena[i].rotacao.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, cena[i].rotacao.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, cena[i].escala);

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glUniform3fv(glGetUniformLocation(shaderProgram, "material_Ka"), 1, glm::value_ptr(cena[i].material.Ka));
            glUniform3fv(glGetUniformLocation(shaderProgram, "material_Kd"), 1, glm::value_ptr(cena[i].material.Kd));
            glUniform3fv(glGetUniformLocation(shaderProgram, "material_Ks"), 1, glm::value_ptr(cena[i].material.Ks));
            glUniform1f(glGetUniformLocation(shaderProgram, "material_Ns"), cena[i].material.Ns);

            GLuint locColor = glGetUniformLocation(shaderProgram, "highlightColor");
            if (i == objSelecionado) { glUniform3f(locColor, 1.2f, 1.2f, 0.5f); } 
            else { glUniform3f(locColor, 1.0f, 1.0f, 1.0f); } 

            if (cena[i].textureID != 0) glBindTexture(GL_TEXTURE_2D, cena[i].textureID);

            glBindVertexArray(cena[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, cena[i].nVertices);
        }
        
        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    for (int i = 0; i < cena.size(); i++) glDeleteVertexArrays(1, &cena[i].VAO);
    glfwTerminate();
    return 0;
}