
//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"
#include "VertexBuffer.h"

#include <GL/glew.h>
#include <GL/gl.h>

int VertexBuffer::totalBufferSize = 0;

VertexBuffer::VertexBuffer() = default;

void VertexBuffer::Init(int bytesize) {
  if (GLEW_ARB_vertex_buffer_object) {
    data = nullptr;
    glGenBuffersARB(1, &id);
  } else {
    data = new char[bytesize];
  }
  size = bytesize;
  totalBufferSize += size;
}

VertexBuffer::~VertexBuffer() {
  if (id != 0U) {
    glDeleteBuffersARB(1, &id);
    id = 0;
  } else {
    delete[] static_cast<char*>(data);
  }
  data = nullptr;
  totalBufferSize -= size;
}

void* VertexBuffer::LockData() {
  if (id != 0U) {
    glBindBufferARB(type, id);
    glBufferDataARB(type, size, nullptr, GL_STATIC_DRAW_ARB);
    return glMapBufferARB(type, GL_WRITE_ONLY);
  }
  return data;
}

void VertexBuffer::UnlockData() const {
  if (id != 0U) {
    glUnmapBufferARB(type);
  }
}

void* VertexBuffer::Bind() {
  if (id != 0U) {
    glBindBufferARB(type, id);
    return nullptr;
  }
  return data;
}

void VertexBuffer::Unbind() const {
  if (id != 0U) {
    glBindBufferARB(type, 0);
  }
}

IndexBuffer::IndexBuffer() { type = GL_ELEMENT_ARRAY_BUFFER_ARB; }
