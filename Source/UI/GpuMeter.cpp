#include "UI/GpuMeter.h"
#include "UI/GainReductionDisplay.h"
#include "UI/Layout.h"

namespace heat::ui
{
    using namespace juce::gl;
    using namespace heat::ui::layout;

    namespace
    {
        const char* vertexShader = R"(
            attribute vec2 position;
            void main()
            {
                gl_Position = vec4 (position, 0.0, 1.0);
            }
        )";

        // Mirrors GainReductionDisplay::paintDynamic / paintColumn / paintNeedle.
        // Colours are unpremultiplied RGBA interpolated like juce::ColourGradient,
        // then premultiplied and composited with "over".
        const char* fragmentShader = R"(
            uniform vec2 uOrigin;
            uniform float uScale;
            uniform sampler2D uBackground;
            uniform vec4 uBgRect;
            uniform sampler2D uRamp;
            uniform vec4 uGlass;
            uniform float uGlassRadius;
            uniform vec4 uColumnsX;
            uniform vec2 uColumnY;
            uniform vec2 uLevel;
            uniform vec2 uK;
            uniform float uKGlobal;
            uniform float uSpillTop;
            uniform vec2 uBloomTop;
            uniform float uPeakY;
            uniform vec2 uNeedleX;

            float px;

            float sdBox (vec2 p, vec4 r, float rad)
            {
                rad = min (rad, 0.5 * min (r.z, r.w));
                vec2 c = r.xy + 0.5 * r.zw;
                vec2 q = abs (p - c) - 0.5 * r.zw + vec2 (rad);
                return length (max (q, 0.0)) + min (max (q.x, q.y), 0.0) - rad;
            }

            float cover (float d)          { return clamp (0.5 - d / px, 0.0, 1.0); }
            float coverStroke (float d, float halfWidth) { return clamp (0.5 - (abs (d) - halfWidth) / px, 0.0, 1.0); }
            vec4 over (vec4 src, vec4 dst) { return src + dst * (1.0 - src.a); }
            vec4 paint (vec4 c, float coverage) { return vec4 (c.rgb * (c.a * coverage), c.a * coverage); }

            vec4 grad2 (float y, float y0, vec4 c0, float y1, vec4 c1)
            {
                return mix (c0, c1, clamp ((y - y0) / (y1 - y0), 0.0, 1.0));
            }

            vec4 grad3 (float y, float y0, vec4 c0, float mid, vec4 cm, float y1, vec4 c1)
            {
                float t = clamp ((y - y0) / (y1 - y0), 0.0, 1.0);
                return t < mid ? mix (c0, cm, t / mid) : mix (cm, c1, (t - mid) / (1.0 - mid));
            }

            vec4 rgba (float r, float g, float b, float a) { return vec4 (r / 255.0, g / 255.0, b / 255.0, a); }

            vec4 column (vec2 p, float x0, float x1, float level, float k, bool outerLeft, vec4 dst)
            {
                if (k <= 0.001)
                    return dst;
                float bottom = uColumnY.y;
                float h = max (1.0, bottom - level);
                float r = min (10.0, 0.5 * h);

                // Body: square top, rounded bottom corners.
                float dBody = max (sdBox (p, vec4 (x0, level - r, x1 - x0, h + r), r), level - p.y);
                float body = cover (dBody);
                float t = clamp ((p.y - level) / h, 0.0, 1.0);
                vec4 ramp = texture2D (uRamp, vec2 ((t * 255.0 + 0.5) / 256.0, 0.5));
                dst = over (ramp * (k * body), dst);

                // Ember haze above the level.
                float hazeTop = max (uColumnY.x + 6.0, level - 36.0);
                if (level > hazeTop)
                {
                    float c = cover (sdBox (p, vec4 (x0 + 1.0, hazeTop, x1 - x0 - 2.0, level - hazeTop), 0.0));
                    dst = over (paint (grad2 (p.y, hazeTop, rgba (40.0, 27.0, 27.0, 0.0), level, rgba (54.0, 34.0, 32.0, 0.9 * k)), c), dst);
                }

                // Neon walls (clipped to the body).
                float wallStart = level + 0.15 * h;
                float xo = outerLeft ? x0 : x1 - 3.0;
                float co = cover (sdBox (p, vec4 (xo, level, 3.0, h), 0.0)) * body;
                dst = over (paint (grad3 (p.y, wallStart, rgba (255.0, 160.0, 95.0, 0.0), 0.5, rgba (250.0, 140.0, 80.0, 0.55 * 0.95 * k),
                                          bottom, rgba (255.0, 225.0, 190.0, 0.95 * k)), co), dst);
                float xi = outerLeft ? x1 - 2.0 : x0;
                float ci = cover (sdBox (p, vec4 (xi, level, 2.0, h), 0.0)) * body;
                dst = over (paint (grad3 (p.y, wallStart, rgba (255.0, 160.0, 95.0, 0.0), 0.5, rgba (250.0, 140.0, 80.0, 0.55 * 0.45 * k),
                                          bottom, rgba (255.0, 225.0, 190.0, 0.45 * k)), ci), dst);

                // White-hot base: the body outline, only near the bottom.
                float clipTop = floor (bottom - 1.6 * r + 0.5);
                float clipBottom = floor (bottom - 1.6 * r + 0.5) + floor (2.0 * r + 0.5);
                vec4 clipRect = vec4 (floor (x0 - 2.0 + 0.5), clipTop, floor (x1 - x0 + 4.0 + 0.5), clipBottom - clipTop);
                float cc = cover (sdBox (p, clipRect, 0.0)) * coverStroke (dBody, 1.2);
                dst = over (paint (grad2 (p.y, bottom - 1.6 * r, rgba (255.0, 236.0, 210.0, 0.0), bottom, rgba (255.0, 246.0, 232.0, 0.95 * k)), cc), dst);
                return dst;
            }

            void main()
            {
                px = 1.0 / uScale;
                vec2 p = vec2 ((gl_FragCoord.x - uOrigin.x) / uScale, (uOrigin.y - gl_FragCoord.y) / uScale);

                vec2 uv = vec2 ((p.x - uBgRect.x) / uBgRect.z, 1.0 - (p.y - uBgRect.y) / uBgRect.w);
                vec4 dst = texture2D (uBackground, uv);

                vec4 dyn = vec4 (0.0);
                float glassBottom = uGlass.y + uGlass.w;
                if (uKGlobal > 0.001)
                {
                    float cs = cover (sdBox (p, vec4 (uGlass.x, uSpillTop, uGlass.z, glassBottom - uSpillTop), 0.0));
                    dyn = over (paint (grad2 (p.y, uSpillTop, rgba (150.0, 95.0, 68.0, 0.0), glassBottom, rgba (150.0, 95.0, 68.0, 0.30 * uKGlobal)), cs), dyn);

                    for (int c = 0; c < 2; ++c)
                    {
                        float x0 = c == 0 ? uColumnsX.x : uColumnsX.z;
                        float x1 = c == 0 ? uColumnsX.y : uColumnsX.w;
                        float top = c == 0 ? uBloomTop.x : uBloomTop.y;
                        vec4 bloom = grad2 (p.y, top, rgba (255.0, 130.0, 60.0, 0.0), uColumnY.y, rgba (255.0, 130.0, 60.0, 0.07 * uKGlobal));
                        for (int i = 1; i <= 3; ++i)
                        {
                            float e = 3.5 * float (i);
                            float hgt = uColumnY.y - top + e;
                            if (hgt > 0.0)
                                dyn = over (paint (bloom, cover (sdBox (p, vec4 (x0 - e, top, x1 - x0 + 2.0 * e, hgt), 9.0 + e))), dyn);
                        }
                    }
                }

                dyn = column (p, uColumnsX.x, uColumnsX.y, uLevel.x, uK.x, true, dyn);
                dyn = column (p, uColumnsX.z, uColumnsX.w, uLevel.y, uK.y, false, dyn);

                if (uKGlobal > 0.001)
                {
                    float d = sdBox (p, vec4 (uGlass.x + 0.8, uGlass.y + 0.8, uGlass.z - 1.6, uGlass.w - 1.6), uGlassRadius - 0.8);
                    dyn = over (paint (grad3 (p.y, uGlass.y, rgba (255.0, 160.0, 90.0, 0.05 * uKGlobal), 0.45, rgba (255.0, 150.0, 80.0, 0.35 * uKGlobal),
                                              glassBottom, rgba (255.0, 200.0, 150.0, 0.85 * uKGlobal)), coverStroke (d, 0.8)), dyn);
                }

                dst = over (dyn * cover (sdBox (p, uGlass, uGlassRadius)), dst);

                if (uPeakY > 0.0)
                {
                    float w = uNeedleX.y - uNeedleX.x;
                    dst = over (paint (rgba (255.0, 140.0, 60.0, 0.18), cover (sdBox (p, vec4 (uNeedleX.x, uPeakY - 5.0, w, 10.0), 0.0))), dst);
                    dst = over (paint (rgba (255.0, 150.0, 70.0, 0.45), cover (sdBox (p, vec4 (uNeedleX.x, uPeakY - 2.2, w, 4.4), 0.0))), dst);
                    dst = over (paint (rgba (255.0, 242.0, 224.0, 1.0), cover (sdBox (p, vec4 (uNeedleX.x, uPeakY - 1.1, w, 2.2), 1.1))), dst);
                }

                gl_FragColor = dst;
            }
        )";

        juce::Image makeRamp()
        {
            // Same stops as GainReductionDisplay::paintColumn.
            auto c = [] (int r, int g, int b) { return juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b); };
            juce::ColourGradient grad (c (54, 34, 32), 0.0f, 0.0f, c (255, 240, 215), 0.0f, 1.0f, false);
            grad.addColour (0.088, c (71, 41, 33));
            grad.addColour (0.176, c (92, 51, 37));
            grad.addColour (0.265, c (118, 63, 41));
            grad.addColour (0.353, c (143, 75, 44));
            grad.addColour (0.44, c (165, 85, 46));
            grad.addColour (0.53, c (192, 100, 54));
            grad.addColour (0.62, c (212, 114, 61));
            grad.addColour (0.706, c (231, 127, 69));
            grad.addColour (0.794, c (249, 149, 82));
            grad.addColour (0.88, c (253, 177, 113));
            grad.addColour (0.97, c (251, 225, 184));
            juce::Image ramp (juce::Image::ARGB, 256, 1, false);
            for (int i = 0; i < 256; ++i)
                ramp.setPixelAt (i, 0, grad.getColourAtPosition (static_cast<double> (i) / 255.0));
            return ramp;
        }
    }

    GpuMeterRenderer::GpuMeterRenderer (juce::OpenGLContext& c) : context (c) {}
    GpuMeterRenderer::~GpuMeterRenderer() = default;

    juce::Rectangle<float> GpuMeterRenderer::captureBounds()
    {
        return GainReductionDisplay::glassBounds();
    }

    void GpuMeterRenderer::setPlacement (float s, int w, int h) noexcept
    {
        canvasScale.store (s);
        componentW.store (w);
        componentH.store (h);
    }

    void GpuMeterRenderer::setState (float l, float r, float p) noexcept
    {
        left.store (l);
        right.store (r);
        peak.store (p);
    }

    void GpuMeterRenderer::requestGlassCapture (std::function<void (juce::Image)> callback)
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        glassCapture = std::move (callback);
    }

    void GpuMeterRenderer::requestFrameCapture (std::function<void (juce::Image)> callback)
    {
        const juce::SpinLock::ScopedLockType lock (captureLock);
        frameCapture = std::move (callback);
        frameCaptureCountdown = 2; // read once a complete frame has been presented
    }

    void GpuMeterRenderer::newOpenGLContextCreated()
    {
        program = std::make_unique<juce::OpenGLShaderProgram> (context);
        const bool ok = program->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader))
                     && program->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShader))
                     && program->link();
        if (! ok)
        {
            DBG ("GPU meter shader: " << program->getLastError());
            program.reset();
        }
        else
        {
            rampTexture.loadImage (makeRamp());
            context.extensions.glGenBuffers (1, &vertexBuffer);
        }
        backgroundScale = 0.0f;
        ready.store (ok);
        if (onReady)
            onReady (ok);
    }

    void GpuMeterRenderer::openGLContextClosing()
    {
        ready.store (false);
        program.reset();
        rampTexture.release();
        backgroundTexture.release();
        if (vertexBuffer != 0)
            context.extensions.glDeleteBuffers (1, &vertexBuffer);
        vertexBuffer = 0;
        backgroundScale = 0.0f;
    }

    void GpuMeterRenderer::buildBackground (float pxPerRef)
    {
        // Snap the texture origin to the pixel grid so texels sit exactly on pixels.
        const auto glass = GainReductionDisplay::glassBounds();
        const float x = std::floor (glass.getX() * pxPerRef - 2.0f) / pxPerRef;
        const float y = std::floor (glass.getY() * pxPerRef - 2.0f) / pxPerRef;
        const int w = static_cast<int> (std::ceil ((glass.getRight() - x) * pxPerRef + 2.0f));
        const int h = static_cast<int> (std::ceil ((glass.getBottom() - y) * pxPerRef + 2.0f));
        juce::Image image (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::translation (-x, -y).scaled (pxPerRef));
            GainReductionDisplay::paintBackground (g);
        }
        backgroundTexture.loadImage (image);
        backgroundRect = { x, y, static_cast<float> (w) / pxPerRef, static_cast<float> (h) / pxPerRef };
        backgroundScale = pxPerRef;
    }

    void GpuMeterRenderer::renderOpenGL()
    {
        GLint viewport[4] {};
        glGetIntegerv (GL_VIEWPORT, viewport);
        const int widthPx = viewport[2], heightPx = viewport[3];
        if (widthPx <= 0 || heightPx <= 0)
            return;

        // Frame capture: before this frame is drawn, the framebuffer still holds
        // the previous, fully composited frame on drivers whose swap preserves
        // it (a copy-swap, as with Mesa under X11); verification only.
        std::function<void (juce::Image)> frameCallback;
        {
            const juce::SpinLock::ScopedLockType lock (captureLock);
            if (frameCapture != nullptr && --frameCaptureCountdown <= 0)
                frameCallback = std::exchange (frameCapture, nullptr);
        }
        {
            if (frameCallback != nullptr)
            {
                std::vector<juce::uint8> pixels (static_cast<size_t> (widthPx * heightPx * 4));
                glPixelStorei (GL_PACK_ALIGNMENT, 1);
                glReadPixels (0, 0, widthPx, heightPx, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                juce::Image image (juce::Image::ARGB, widthPx, heightPx, false);
                for (int yy = 0; yy < heightPx; ++yy)
                    for (int xx = 0; xx < widthPx; ++xx)
                    {
                        const auto* px = pixels.data() + ((heightPx - 1 - yy) * widthPx + xx) * 4;
                        image.setPixelAt (xx, yy, juce::Colour (px[0], px[1], px[2], px[3]));
                    }
                frameCallback (image);
            }
        }

        juce::OpenGLHelpers::clear (juce::Colours::transparentBlack);
        if (program == nullptr)
            return;

        const float renderScale = static_cast<float> (context.getRenderingScale());
        const float pxPerRef = canvasScale.load() * renderScale;
        if (std::abs (pxPerRef - backgroundScale) > 1.0e-3f)
            buildBackground (pxPerRef);

        const auto glass = GainReductionDisplay::glassBounds();
        const float l = left.load(), r = right.load(), pk = peak.load();
        const float kGlobal = GainReductionDisplay::heatFor (std::min (l, r));

        program->use();
        auto uniform = [this] (const char* name) { return juce::OpenGLShaderProgram::Uniform (*program, name); };
        uniform ("uOrigin").set (0.0f, static_cast<float> (heightPx));
        uniform ("uScale").set (pxPerRef);
        uniform ("uBgRect").set (backgroundRect.getX(), backgroundRect.getY(), backgroundRect.getWidth(), backgroundRect.getHeight());
        uniform ("uGlass").set (glass.getX(), glass.getY(), glass.getWidth(), glass.getHeight());
        uniform ("uGlassRadius").set (GainReductionDisplay::glassRadius());
        uniform ("uColumnsX").set (meterLeftBarX0, meterScaleX0, meterScaleX1, meterRightBarX1);
        uniform ("uColumnY").set (meterColumnTop, meterColumnBottom);
        uniform ("uLevel").set (GainReductionDisplay::yForDb (l), GainReductionDisplay::yForDb (r));
        uniform ("uK").set (GainReductionDisplay::heatFor (l), GainReductionDisplay::heatFor (r));
        uniform ("uKGlobal").set (kGlobal);
        uniform ("uSpillTop").set (GainReductionDisplay::yForDb (std::min (l, r)));
        uniform ("uBloomTop").set (GainReductionDisplay::yForDb (l) + 30.0f, GainReductionDisplay::yForDb (r) + 30.0f);
        uniform ("uPeakY").set (pk < -0.05f ? GainReductionDisplay::yForDb (pk) : -1.0f);
        uniform ("uNeedleX").set (meterScaleX1 + 1.5f, meterRightBarX1 - 1.0f);

        context.extensions.glActiveTexture (GL_TEXTURE0);
        backgroundTexture.bind();
        uniform ("uBackground").set (0);
        context.extensions.glActiveTexture (GL_TEXTURE1);
        rampTexture.bind();
        uniform ("uRamp").set (1);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        context.extensions.glActiveTexture (GL_TEXTURE0);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // One quad over the glass (+2 px), in normalised device coordinates.
        const auto area = glass.expanded (2.0f / pxPerRef);
        auto ndcX = [&] (float x) { return 2.0f * x * pxPerRef / static_cast<float> (widthPx) - 1.0f; };
        auto ndcY = [&] (float y) { return 1.0f - 2.0f * y * pxPerRef / static_cast<float> (heightPx); };
        const GLfloat quad[] = { ndcX (area.getX()), ndcY (area.getY()),      ndcX (area.getRight()), ndcY (area.getY()),
                                 ndcX (area.getX()), ndcY (area.getBottom()), ndcX (area.getRight()), ndcY (area.getBottom()) };
        context.extensions.glBindBuffer (GL_ARRAY_BUFFER, vertexBuffer);
        context.extensions.glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STREAM_DRAW);
        const auto position = static_cast<GLuint> (context.extensions.glGetAttribLocation (program->getProgramID(), "position"));
        context.extensions.glVertexAttribPointer (position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        context.extensions.glEnableVertexAttribArray (position);
        glDisable (GL_BLEND);
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        context.extensions.glDisableVertexAttribArray (position);
        context.extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
        rampTexture.unbind();
        backgroundTexture.unbind();

        // Glass capture (verification): read back exactly what was just drawn.
        std::function<void (juce::Image)> glassCallback;
        {
            const juce::SpinLock::ScopedLockType lock (captureLock);
            glassCallback = std::exchange (glassCapture, nullptr);
        }
        if (glassCallback != nullptr)
        {
            const int x0 = static_cast<int> (std::floor (glass.getX() * pxPerRef));
            const int y0 = static_cast<int> (std::floor (glass.getY() * pxPerRef));
            const int w = static_cast<int> (std::ceil (glass.getRight() * pxPerRef)) - x0;
            const int h = static_cast<int> (std::ceil (glass.getBottom() * pxPerRef)) - y0;
            std::vector<juce::uint8> pixels (static_cast<size_t> (w * h * 4));
            glPixelStorei (GL_PACK_ALIGNMENT, 1);
            glReadPixels (x0, heightPx - y0 - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            juce::Image image (juce::Image::ARGB, w, h, false);
            for (int yy = 0; yy < h; ++yy)
                for (int xx = 0; xx < w; ++xx)
                {
                    const auto* px = pixels.data() + ((h - 1 - yy) * w + xx) * 4;
                    image.setPixelAt (xx, yy, juce::Colour (px[0], px[1], px[2], px[3]));
                }
            glassCallback (image);
        }
    }
}
