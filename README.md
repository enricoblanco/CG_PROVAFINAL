# Visualizador 3D Interativo - Avaliação Final (Computação Gráfica)

**Autor:** Enrico Blanco  
**Instituição:** Unisinos  

Este repositório contém o projeto final da disciplina de Computação Gráfica. Trata-se de um visualizador 3D desenvolvido em C++ moderno e OpenGL, projetado para renderizar múltiplas malhas poligonais simultaneamente, com suporte a iluminação de Phong, materiais (.mtl), texturização, câmera livre interativa e animação baseada em curvas paramétricas.

---

## ⚙️ Setup e Instruções de Compilação

O projeto foi estruturado utilizando o **CMake** para gerenciar as dependências de forma automática e independente do sistema operacional, baixando as bibliotecas em tempo de compilação.

### Dependências Utilizadas
* **GLFW (3.4):** Gerenciamento de janelas e captura de eventos de hardware (mouse e teclado).
* **GLAD:** Carregador de ponteiros de funções do OpenGL Core Profile.
* **GLM (OpenGL Mathematics):** Biblioteca matemática para cálculos de matrizes e vetores (Matrizes Model, View e Projection).
* **stb_image:** Biblioteca *single-header* para decodificação e carregamento de texturas em memória.

### Como Compilar e Executar (Ambiente macOS / VS Code)
1. Certifique-se de ter o **CMake** e as extensões **C/C++** e **CMake Tools** instaladas no seu Visual Studio Code.
2. Clone este repositório e abra a pasta raiz no VS Code.
3. Pressione `Cmd + Shift + P`, digite `CMake: Select a Kit` e selecione o compilador nativo (**Clang arm64**).
4. O CMake irá configurar o projeto e baixar as dependências (`FetchContent`) automaticamente.
5. Na barra inferior do VS Code, clique em **Build** (Compilar).
6. Após a compilação, clique no botão de **Play / Launch** para executar o arquivo `VisualizadorFinal`.

---

## 🎮 Controles da Aplicação

O visualizador exige atalhos de teclado para navegação e manipulação dinâmica da cena em tempo real:

* **Mouse:** Direciona a visão da Câmera Livre (Estilo FPS).
* **Setas do Teclado:** Movimentação da Câmera pelo cenário (Frente, Trás, Esquerda, Direita).
* **TAB:** Alterna o objeto atualmente selecionado (o objeto selecionado recebe um destaque visual na cor amarela).
* **Teclas 1, 2 e 3:** Altera o modo de transformação mecânica do objeto selecionado (1 = Translação, 2 = Rotação, 3 = Escala).
* **Teclas W, A, S, D:** Aplica a transformação no objeto selecionado com base no modo ativo (1, 2 ou 3).
* **Barra de Espaço:** Inicia ou pausa a animação paramétrica (Curva de Bézier Cúbica) no objeto configurado.
* **Teclas 7, 8 e 9:** Alternam individualmente as fontes de iluminação (7 = Luz Principal, 8 = Luz de Preenchimento, 9 = Contra-luz).
* **ESC:** Libera o cursor do mouse e encerra a aplicação com segurança.

---

## 📦 Assets e Procedência

Os modelos 3D e texturas utilizados nesta cena foram obtidos de fontes de domínio público ou gerados sob licenças livres.

* **Modelos 3D:**
  * **Suzanne (Macaco):** Geometria padrão primitiva gerada através do software **Blender**. Exportada utilizando os parâmetros normais de geometria (`.obj` e `.mtl`). Licença: Domínio Público / GNU GPL.

*Todos os recursos encontram-se organizados na pasta `/assets`, juntamente com o arquivo `cena.txt` utilizado pelo parser customizado para inicializar e instanciar o ambiente.*

---

## 📚 Referências e Bibliografia

O desenvolvimento deste software foi fundamentado nos seguintes materiais e documentações técnicas consultados durante o semestre:

1. **Repositório Oficial da Disciplina:** Material didático, templates estruturais e tutoriais fornecidos pelo professor Guilherme Chagas Kurtz (Unisinos).
2. **LearnOpenGL:** Vries, Joey de. Documentação aprofundada sobre o pipeline gráfico do OpenGL moderno, transformações lineares e shading de Phong. Disponível em: https://learnopengl.com/
3. **Documentação GLM:** Manual de uso da API de álgebra linear voltada para rendering 3D. Disponível em: https://glm.g-truc.net/
4. **Documentação GLFW:** Guia de referência para gerenciamento de contextos OpenGL, janelas nativas e tratamento de inputs em tempo real. Disponível em: https://www.glfw.org/docs/latest/
5. **Fundamentos de Curvas Paramétricas:** Formulação matemática e polinômios de Bernstein aplicados no desenvolvimento computacional de trajetórias por Curvas de Bézier.