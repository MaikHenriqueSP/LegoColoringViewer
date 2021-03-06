#ifndef OPENGLWINDOW_HPP_
#define OPENGLWINDOW_HPP_

#include "abcg.hpp"
#include "body_parts.hpp"
#include <tiny_obj_loader.h>

struct Vertex {
  glm::vec3 position;

  bool operator==(const Vertex& other) const {
    return position == other.position;
  }
};

struct Shape {
  std::string name;
  ulong verticesNumber;
  bool isActive;
  Type type;
};

class OpenGLWindow : public abcg::OpenGLWindow {
 protected:
  void initializeGL() override;
  void paintGL() override;
  void paintUI() override;
  void resizeGL(int width, int height) override;
  void terminateGL() override;
  glm::mat4 getRotation();

 private:
  GLuint m_VAO{};
  GLuint m_VBO{};
  GLuint m_EBO{};
  GLuint m_program{};
  int m_viewportWidth{};
  int m_viewportHeight{};
  float m_angle{};
  int m_verticesToDraw{0};
  int m_totalVertices{0};
  int m_drawIntervalMilliseconds{30};
  int m_incrementedVerticesPerTime{30};

  glm::mat4 m_modelMatrix{1.0f};
  glm::mat4 m_viewMatrix{1.0f};
  glm::mat4 m_projMatrix{1.0f};

  void update();
  
  glm::mat4 m_rotation{1.0f};

  abcg::ElapsedTimer m_drawCoolDown;

  std::vector<Vertex> m_vertices;
  std::vector<Shape> m_shapes;
  std::vector<GLuint> m_indices;
  glm::vec4 m_skinColor;
  glm::vec4 m_lowerClothesColor;
  glm::vec4 m_upperClothesColor;

  void loadModelFromFile(std::string_view path);
  void standardize();
  void setColor(Type type);
  Shape getShape(tinyobj::shape_t shape);
  void renderShape();
  void configureBuffers(GLuint* buffers, GLenum target, 	const void* data, GLsizeiptr size);
  void generateModel(tinyobj::ObjReader reader);
};

#endif