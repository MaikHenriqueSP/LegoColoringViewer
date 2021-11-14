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
  GLulong verticesNumber;
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
};

#endif