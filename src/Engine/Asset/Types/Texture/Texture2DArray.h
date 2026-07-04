#pragma once
#include "Texture.h"

class Texture2DArray : public Texture
{
  public:
    virtual bool Create(const TextureDesc& desc, const void* pixelData = nullptr) override;
    virtual bool Resize(int width, int height, int channels = 0) override;

    virtual void UnBind() override;
    virtual void Bind(unsigned int uint) override;

  private:
    unsigned int m_LayerCount;
};