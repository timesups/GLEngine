GLSLShader
{
    Properties
    {
        
    }
    SubShader
    {
        Pass
        { 
            GLSLPROGRAM
            #ifdef VERTEX
            #include "/include/Core.glsl"
            
            void main()
            {
                gl_Position = ObjectToClipPos(aPosition);
            }
            #endif

            #ifdef FRAGMENT

            #ifdef DEFERREDRENDER
                layout (location = 0) out vec4 _AlbdeoAO;
                layout (location = 1) out vec4  _NormalXY;
                layout (location = 2) out vec4  _MRSC;
                layout (location = 3) out vec4  _Flag;
            #endif
            
            #ifdef FORWARDRENDER
                out vec4 FragColor;
            #endif
            void main() 
            {
                vec4 color = vec4(1.0,0.0,1.0 ,1.0);
            #ifdef DEFERREDRENDER
                _AlbdeoAO = color;
                _Flag = vec4(0.0,1,1,1);
            #endif


            #ifdef FORWARDRENDER
                FragColor = color;
            #endif
            }
            #endif
            ENDGLSL
        }
    }
}