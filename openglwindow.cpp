#include "openglwindow.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <tiny_obj_loader.h>

#include <cppitertools/itertools.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

namespace std {
template <>
struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const noexcept {
    std::size_t h1{std::hash<glm::vec3>()(vertex.position)};
    return h1;
  }
};
}

void OpenGLWindow::initializeGL() {
  glClearColor(1, 1, 1, 1);

  glEnable(GL_DEPTH_TEST);

  m_program = createProgramFromFile(getAssetsPath() + "loadmodel.vert", getAssetsPath() + "loadmodel.frag");

  loadModelFromFile(getAssetsPath() + "lego obj.obj");
  standardize();

  m_verticesToDraw = m_indices.size();

  glGenBuffers(1, &m_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices[0]) * m_vertices.size(), m_vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1, &m_EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_indices[0]) * m_indices.size(),
               m_indices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  glGenVertexArrays(1, &m_VAO);

  glBindVertexArray(m_VAO);

  glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  GLint positionAttribute{glGetAttribLocation(m_program, "inPosition")};
  glEnableVertexAttribArray(positionAttribute);
  glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex), nullptr);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

  glBindVertexArray(0);
}

void OpenGLWindow::loadModelFromFile(std::string_view path) {
  tinyobj::ObjReaderConfig readerConfig;
  readerConfig.mtl_search_path =
      getAssetsPath() + "mtl/";  // Path to material files

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path.data(), readerConfig)) {
    if (!reader.Error().empty()) {
      throw abcg::Exception{abcg::Exception::Runtime(
          fmt::format("Failed to load model {} ({})", path, reader.Error()))};
    }
    throw abcg::Exception{
        abcg::Exception::Runtime(fmt::format("Failed to load model {}", path))};
  }

  if (!reader.Warning().empty()) {
    fmt::print("Warning: {}\n", reader.Warning());
  }

  const auto& attrib{reader.GetAttrib()};
  const auto& shapes{reader.GetShapes()};

  m_vertices.clear();
  m_indices.clear();

  // A key:value map with key=Vertex and value=index
  std::unordered_map<Vertex, GLuint> hash{};

  for (const auto& shape : shapes) {

    size_t indexOffset{0};
    GLuint nameTypeDivisionIndex = shape.name.find("-");

    std::string shapeName = shape.name.substr(0, nameTypeDivisionIndex);
    std::string shapeType = shape.name.substr(nameTypeDivisionIndex + 1);

    int shapeNumber = std::stoi(shapeType);
    Type type = Type(shapeNumber);
    
    Shape mappedShaped = Shape{
      .name = shapeName,
      .verticesNumber = shape.mesh.indices.size(),
      .isActive = true,
      .type = type
    };
    
    m_shapes.push_back(mappedShaped);

    for (const auto faceNumber : iter::range(shape.mesh.num_face_vertices.size())) {
      std::size_t numFaceVertices{shape.mesh.num_face_vertices[faceNumber]};
      std::size_t startIndex{};
      for (const auto vertexNumber : iter::range(numFaceVertices)) {
        tinyobj::index_t index{shape.mesh.indices[indexOffset + vertexNumber]};

        startIndex = 3 * index.vertex_index;
        tinyobj::real_t vx = attrib.vertices[startIndex + 0];
        tinyobj::real_t vy = attrib.vertices[startIndex + 1];
        tinyobj::real_t vz = attrib.vertices[startIndex + 2];

        Vertex vertex{};
        vertex.position = {vx, vy, vz};

        if (hash.count(vertex) == 0) {
          hash[vertex] = m_vertices.size();
          m_vertices.push_back(vertex);
        }

        m_indices.push_back(hash[vertex]);
      }
      indexOffset += numFaceVertices;
    }
  }

}

void OpenGLWindow::standardize() {

  glm::vec3 max(std::numeric_limits<float>::lowest());
  glm::vec3 min(std::numeric_limits<float>::max());
  for (const auto& vertex : m_vertices) {
    max.x = std::max(max.x, vertex.position.x);
    max.y = std::max(max.y, vertex.position.y);
    max.z = std::max(max.z, vertex.position.z);
    min.x = std::min(min.x, vertex.position.x);
    min.y = std::min(min.y, vertex.position.y);
    min.z = std::min(min.z, vertex.position.z);
  }

  const auto center{(min + max) / 2.0f};
  const auto scaling{2.0f / glm::length(max - min)};
  for (auto& vertex : m_vertices) {
    vertex.position = (vertex.position - center) * scaling;
  }
}

void OpenGLWindow::paintGL() {
  float deltaTime{static_cast<float>(getDeltaTime())};
  m_angle = glm::wrapAngle(m_angle + glm::radians(15.0f) * deltaTime);
  GLint colorLoc{glGetUniformLocation(m_program, "color")};

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glViewport(0, 0, m_viewportWidth, m_viewportHeight);

  glUseProgram(m_program);
  glBindVertexArray(m_VAO);

  GLint angleLoc{glGetUniformLocation(m_program, "angle")};
  glUniform1f(angleLoc, m_angle);

  GLulong previous = 0;
  for (auto& shape : m_shapes) {
    if (shape.isActive) {
      setColor(shape.type);

      glDrawElements(GL_TRIANGLES, shape.verticesNumber, GL_UNSIGNED_INT, (void*)(previous * sizeof(GLuint)));
    }
    previous += shape.verticesNumber;    
  }

  glBindVertexArray(0);
  glUseProgram(0);
}

void OpenGLWindow::paintUI() {
  abcg::OpenGLWindow::paintUI();

  {
    auto widgetSize{ImVec2(172, 332)};
    ImGui::SetNextWindowPos(ImVec2(m_viewportWidth - widgetSize.x - 5, 5));
    ImGui::SetNextWindowSize(widgetSize);
    ImGui::Begin("Widget window", nullptr, ImGuiWindowFlags_NoDecoration);

    static bool faceCulling{};
    ImGui::Checkbox("Back-face culling", &faceCulling);

    for (auto& shape : m_shapes) {
      ImGui::Checkbox(shape.name.c_str(), &shape.isActive);
    }

    auto colorEditFlags{ImGuiColorEditFlags_PickerHueBar};
    ImGui::ColorEdit3("Skin Color", &m_skinColor.x, colorEditFlags);      
    ImGui::ColorEdit3("Lower Clothes Color", &m_lowerClothesColor.x, colorEditFlags);      
    ImGui::ColorEdit3("Upper Clothes Color", &m_upperClothesColor.x, colorEditFlags);      

    ImGui::End();
  }
}

void OpenGLWindow::resizeGL(int width, int height) {
  m_viewportWidth = width;
  m_viewportHeight = height;
}

void OpenGLWindow::terminateGL() {
  glDeleteProgram(m_program);
  glDeleteBuffers(1, &m_EBO);
  glDeleteBuffers(1, &m_VBO);
  glDeleteVertexArrays(1, &m_VAO);
}

void OpenGLWindow::setColor(Type type) {
  GLint colorLoc{glGetUniformLocation(m_program, "color")};
  switch (type) {
      case Type::Skin:
        glUniform4f(colorLoc, m_skinColor[0], m_skinColor[1], m_skinColor[2], m_skinColor[3]);        
        break;
      case Type::LowerClothes:
        glUniform4f(colorLoc, m_lowerClothesColor[0], m_lowerClothesColor[1], m_lowerClothesColor[2], m_lowerClothesColor[3]);
        break;
      case Type::UpperClothes:
        glUniform4f(colorLoc, m_upperClothesColor[0], m_upperClothesColor[1], m_upperClothesColor[2], m_upperClothesColor[3]);
        break;      
    }
}