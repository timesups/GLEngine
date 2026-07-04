#pragma once
#include "Texture.h"
#include <memory>

class TextureCube : public Texture
{
  public:
    virtual bool Create(const TextureDesc& desc, const void* pixelData = nullptr) override;
    virtual bool Resize(int width, int height, int channels = 0) override;

    virtual void UnBind() override;
    virtual void Bind(unsigned int uint) override;
};
