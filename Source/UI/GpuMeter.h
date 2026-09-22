#pragma once

#include <juce_opengl/juce_opengl.h>

#include <atomic>
#include <functional>

namespace heat::ui
{
    // OpenGL renderer for the gain-reduction meter's glass interior.
    //
    // Attached (through a juce::OpenGLContext) to the editor. JUCE draws
    // renderOpenGL() first and then blends the software-painted component
    // layer on top; in GPU mode the chassis and the meter component leave the
    // glass interior transparent, so what shows there is drawn here:
    //
    //   glass background     texture, rendered once per scale by the same code
    //                        as the CPU meter (GainReductionDisplay::paintBackground)
    //   warmth, bloom, ember columns (ramp from a lookup texture), neon walls,
    //   white-hot base, rim glow, peak needle
    //                        one fragment shader, shapes as signed-distance
    //                        fields with pixel-accurate antialiasing
    //
    // The scale overlay (centre column, ticks, legends) stays in the component
    // layer above. Per frame the CPU only uploads a handful of uniforms: no
    // path rasterisation, no pixel pushing.
    class GpuMeterRenderer : public juce::OpenGLRenderer
    {
    public:
        explicit GpuMeterRenderer (juce::OpenGLContext& context);
        ~GpuMeterRenderer() override;

        // Message thread: where the 1536 x 1024 design canvas sits in the
        // attached component (uniform scale, top-left origin) and its size.
        void setPlacement (float canvasScale, int componentWidth, int componentHeight) noexcept;
        // Message thread: meter state for the next frame (dB, <= 0).
        void setState (float leftDb, float rightDb, float peakDb) noexcept;

        // Called on the GL thread once the shader is (or failed to be) ready.
        std::function<void (bool ok)> onReady;
        bool isReady() const noexcept { return ready.load(); }

        // Verification hooks (GL thread invokes the callback):
        //  - the glass region exactly as rendered by this renderer
        //  - the complete previous frame including JUCE's component layer
        void requestGlassCapture (std::function<void (juce::Image)> callback);
        void requestFrameCapture (std::function<void (juce::Image)> callback);
        // Reference-space rectangle the glass capture covers.
        static juce::Rectangle<float> captureBounds();

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

    private:
        void buildBackground (float pxPerRef);

        juce::OpenGLContext& context;
        std::unique_ptr<juce::OpenGLShaderProgram> program;
        juce::OpenGLTexture rampTexture, backgroundTexture;
        juce::Rectangle<float> backgroundRect; // reference units covered by the background texture
        float backgroundScale = 0.0f;
        juce::uint32 vertexBuffer = 0;

        std::atomic<float> canvasScale { 1.0f };
        std::atomic<int> componentW { 1536 }, componentH { 1024 };
        std::atomic<float> left { 0.0f }, right { 0.0f }, peak { 0.0f };
        std::atomic<bool> ready { false };

        juce::SpinLock captureLock;
        std::function<void (juce::Image)> glassCapture, frameCapture;
        int frameCaptureCountdown = 0;
    };
}
