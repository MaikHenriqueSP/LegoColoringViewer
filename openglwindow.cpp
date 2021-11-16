#include "openglwindow.hpp"

#include <fmt/core.h>
#include <imgui.h>

#include <cppitertools/itertools.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

namespace std {
  template <> struct hash<Vertex> {
    size_t operator()(Vertex const& vertex) const noexcept {
      std::size_t h1{std::hash<glm::vec3>()(vertex.position)};
      return h1;
    }
  };
}

void OpenGLWindow::initializeGL() {
  glClearColor(1, 1, 1, 1);
  glEnable(GL_DEPTH_TEST);

  m_program = createProgramFromFile(getAssetsPath() + "shader.vert", getAssetsPath() + "shader.frag");
  loadModelFromFile(getAssetsPath() + "lego obj.obj");
  
  standardize();
  
  m_totalVertices = m_indices.size();

  configureBuffers(&m_VBO, GL_ARRAY_BUFFER, m_vertices.data(), sizeof(m_vertices[0]) * m_vertices.size());
  configureBuffers(&m_EBO, GL_ELEMENT_ARRAY_BUFFER, m_indices.data(), sizeof(m_indices[0]) * m_indices.size());

  glGenVertexArrays(1, &m_VAO);
  glBindVertexArray(m_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  
  GLint positionAttribute{glGetAttribLocation(m_program, "inPosition")};
  glEnableVertexAttribArray(positionAttribute);
  glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
  glBindVertexArray(0);
}

void OpenGLWindow::configureBuffers(GLuint* buffers, GLenum target, 	const void* data, GLsizeiptr size) {
  glGenBuffers(1, buffers);
  glBindBuffer(target, *buffers);
  glBufferData(target, size, data, GL_STATIC_DRAW);
  glBindBuffer(target, 0);
}

void OpenGLWindow::loadModelFromFile(std::string_view path) {
  tinyobj::ObjReaderConfig readerConfig;
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

  generateModel(reader);  
}

void OpenGLWindow::generateModel(tinyobj::ObjReader reader) {
  const auto& attrib{reader.GetAttrib()};
  const auto& shapes{reader.GetShapes()};

  m_vertices.clear();
  m_indices.clear();

  std::unordered_map<Vertex, GLuint> hash{};

  for (const auto& shape : shapes) {
    Shape mappedShaped = getShape(shape);    
    m_shapes.push_back(mappedShaped);
    
    size_t indexOffset{0};

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
  update();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glViewport(0, 0, m_viewportWidth, m_viewportHeight);

  glUseProgram(m_program);

  GLint viewMatrixLoc{glGetUniformLocation(m_program, "viewMatrix")};
  GLint projMatrixLoc{glGetUniformLocation(m_program, "projMatrix")};
  GLint modelMatrixLoc{glGetUniformLocation(m_program, "modelMatrix")};

  glUniformMatrix4fv(viewMatrixLoc, 1, GL_FALSE, &m_viewMatrix[0][0]);
  glUniformMatrix4fv(projMatrixLoc, 1, GL_FALSE, &m_projMatrix[0][0]);
  glUniformMatrix4fv(modelMatrixLoc, 1, GL_FALSE, &m_modelMatrix[0][0]);

  glBindVertexArray(m_VAO);

  GLint angleLoc{glGetUniformLocation(m_program, "angle")};
  glUniform1f(angleLoc, m_angle);
  
  renderShape();

  glBindVertexArray(0);
  glUseProgram(0);
}

void OpenGLWindow::renderShape() {
  if (m_drawCoolDown.elapsed() > m_drawIntervalMilliseconds / 1000.0  && m_verticesToDraw <= m_totalVertices) {
    m_verticesToDraw += m_incrementedVerticesPerTime;
    m_drawCoolDown.restart();
  }

  ulong tempVerticesToDraw = m_verticesToDraw;
  ulong shapeOffset = 0;

  for (auto& shape : m_shapes) {
    if (tempVerticesToDraw <= 0) {
      break;
    }

    if (!shape.isActive) {
      shapeOffset += shape.verticesNumber;
      continue;
    }
    
    ulong verticesCount = 0;
    long verticesToDrawOnShape = tempVerticesToDraw - shape.verticesNumber;
    
    if (verticesToDrawOnShape <= 0) {
      verticesCount = tempVerticesToDraw;
      tempVerticesToDraw = 0;
    } else {
      tempVerticesToDraw -= shape.verticesNumber;
      verticesCount = shape.verticesNumber;
    }

    setColor(shape.type);
    glDrawElements(GL_TRIANGLES, verticesCount, GL_UNSIGNED_INT, (void*)(shapeOffset * sizeof(GLuint)));

    shapeOffset += shape.verticesNumber;
  }

}

void OpenGLWindow::paintUI() {
  abcg::OpenGLWindow::paintUI();

  {
    auto widgetSize{ImVec2(170, 570)};
    ImGui::SetNextWindowPos(ImVec2(m_viewportWidth - widgetSize.x - 5, 5));
    ImGui::SetNextWindowSize(widgetSize);
    ImGui::Begin("Widget window", nullptr, ImGuiWindowFlags_NoDecoration);

    for (auto& shape : m_shapes) {
      ImGui::Checkbox(shape.name.c_str(), &shape.isActive);
    }

    ImGui::NewLine();

    auto colorEditFlags{ImGuiColorEditFlags_PickerHueBar};
    ImGui::ColorEdit3("Skin", &m_skinColor.x, colorEditFlags);      
    ImGui::ColorEdit3("Lower", &m_lowerClothesColor.x, colorEditFlags);      
    ImGui::ColorEdit3("Upper", &m_upperClothesColor.x, colorEditFlags);      

    ImGui::NewLine();
    if (ImGui::Button("Restart Draw", ImVec2(155, 30))) {
      m_verticesToDraw = 0;
    }

    ImGui::NewLine();
    ImGui::LabelText("", "Interval in ms");    
    ImGui::SliderInt("", &m_drawIntervalMilliseconds, 0, 100 );

    ImGui::NewLine();
    ImGui::LabelText("", "Vertices/time");    
    ImGui::SliderInt(" ", &m_incrementedVerticesPerTime, 0, 100 );

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

Shape OpenGLWindow::getShape(tinyobj::shape_t shape) {
    GLuint nameTypeDivisionIndex = shape.name.find("-");

    std::string shapeName = shape.name.substr(0, nameTypeDivisionIndex);
    std::string shapeType = shape.name.substr(nameTypeDivisionIndex + 1);

    int shapeNumber = std::stoi(shapeType);
    Type type = Type(shapeNumber);
    
    Shape mappedShape = Shape{
      .name = shapeName,
      .verticesNumber = shape.mesh.indices.size(),
      .isActive = true,
      .type = type
    };
    return mappedShape;
}

glm::mat4 OpenGLWindow::getRotation() {
  glm::vec3 yAxis{0.0f, 1.0f, 0.0f};  
  return glm::rotate(glm::mat4(1.0f), m_angle, yAxis) * m_rotation;;
}

void OpenGLWindow::update() {
  float deltaTime{static_cast<float>(getDeltaTime())};
  m_angle = glm::wrapAngle(m_angle + glm::radians(15.0f) * deltaTime);

  m_modelMatrix = getRotation();
  m_viewMatrix = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.5f),
                glm::vec3(0.0f, 0.0f, 0.0f), 
                glm::vec3(0.0f, 1.0f, 0.0f));
      
  auto aspectRatio{static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight)};
  m_projMatrix =  glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 5.0f);
}