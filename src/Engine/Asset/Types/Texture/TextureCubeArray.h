#pragma once
#include "Texture.h"

class TextureCubeArray : public Texture
{
  public:
    virtual bool Create(const TextureDesc& desc, const void* pixelData = nullptr) override;
    virtual bool Resize(int width, int height, int channels = 0) override;

    virtual void UnBind() override;
    virtual void Bind(unsigned int uint) override;
    unsigned int GetLayerCount() const;

  private:
    unsigned int m_LayerCount;
};