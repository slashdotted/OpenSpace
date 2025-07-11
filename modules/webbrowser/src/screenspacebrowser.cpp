/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2025                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/webbrowser/include/screenspacebrowser.h>

#include <modules/webbrowser/webbrowsermodule.h>
#include <modules/webbrowser/include/webkeyboardhandler.h>
#include <modules/webbrowser/include/browserinstance.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/engine/windowdelegate.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/texture.h>
#include <optional>

namespace {
    constexpr std::string_view _loggerCat = "ScreenSpaceBrowser";

    constexpr openspace::properties::Property::PropertyInfo DimensionsInfo = {
        "Dimensions",
        "Browser Dimensions",
        "The dimensions of the web browser window in pixels.",
        openspace::properties::Property::Visibility::User
    };

    constexpr openspace::properties::Property::PropertyInfo UrlInfo = {
        "Url",
        "URL",
        "The URL to load.",
        openspace::properties::Property::Visibility::NoviceUser
    };

    constexpr openspace::properties::Property::PropertyInfo ReloadInfo = {
        "Reload",
        "Reload",
        "Reload the web browser.",
        openspace::properties::Property::Visibility::NoviceUser
    };

    // This `ScreenSpaceRenderable` can be used to render a webpage in front of the
    // camera. This can be used to show various dynamic content, for example using the
    // scripting API.
    //
    // Note that mouse input will not be passed to the rendered view, so it will not be
    // possible to interact with the web page.
    struct [[codegen::Dictionary(ScreenSpaceBrowser)]] Parameters {
        // A unique identifier for this screen space browser.
        std::optional<std::string> identifier [[codegen::identifier()]];

        // [[codegen::verbatim(UrlInfo.description)]]
        std::optional<std::string> url;

        // [[codegen::verbatim(DimensionsInfo.description)]]
        std::optional<glm::vec2> dimensions [[codegen::greater({ 0, 0 })]];
    };
#include "screenspacebrowser_codegen.cpp"

} // namespace

namespace openspace {

void ScreenSpaceBrowser::ScreenSpaceRenderHandler::draw() {}

void ScreenSpaceBrowser::ScreenSpaceRenderHandler::render() {}

void ScreenSpaceBrowser::ScreenSpaceRenderHandler::setTexture(GLuint t) {
    _texture = t;
}

documentation::Documentation ScreenSpaceBrowser::Documentation() {
    return codegen::doc<Parameters>("core_screenspace_browser");
}

ScreenSpaceBrowser::ScreenSpaceBrowser(const ghoul::Dictionary& dictionary)
    : ScreenSpaceRenderable(dictionary)
    , _dimensions(DimensionsInfo, glm::uvec2(0), glm::uvec2(0), glm::uvec2(3000))
    , _reload(ReloadInfo)
    , _renderHandler(new ScreenSpaceRenderHandler)
    , _url(UrlInfo)
    , _keyboardHandler(new WebKeyboardHandler)
{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    std::string identifier = p.identifier.value_or("ScreenSpaceBrowser");
    identifier = makeUniqueIdentifier(identifier);
    setIdentifier(identifier);

    _url = p.url.value_or(_url);

    _dimensions = p.dimensions.value_or(glm::vec2(1920, 1080));

    _browserInstance = std::make_unique<BrowserInstance>(
        _renderHandler.get(),
        _keyboardHandler.get()
    );

    _url.onChange([this]() { _isUrlDirty = true; });
    _dimensions.onChange([this]() { _isDimensionsDirty = true; });
    _reload.onChange([this]() { _browserInstance->reloadBrowser(); });

    addProperty(_url);
    addProperty(_dimensions);
    addProperty(_reload);
    _useAcceleratedRendering = WebBrowserModule::canUseAcceleratedRendering();

    WebBrowserModule* webBrowser = global::moduleEngine->module<WebBrowserModule>();
    if (webBrowser) {
        webBrowser->addBrowser(_browserInstance.get());
    }
}

void ScreenSpaceBrowser::initializeGL() {
    ScreenSpaceRenderable::initializeGL();

    createShaders();
    _browserInstance->initialize();
    _browserInstance->loadUrl(_url);
}

void ScreenSpaceBrowser::deinitializeGL() {
    LDEBUG(std::format("Deinitializing ScreenSpaceBrowser: {}", _url.value()));

    _browserInstance->close(true);

    WebBrowserModule* webBrowser = global::moduleEngine->module<WebBrowserModule>();
    webBrowser->removeBrowser(_browserInstance.get());
    _browserInstance.reset();

    ScreenSpaceRenderable::deinitializeGL();
}

void ScreenSpaceBrowser::render(const RenderData& renderData) {
    if (!_renderHandler->isTextureReady()) {
        return;
    }

    _renderHandler->updateTexture();

    const glm::mat4 mat =
        globalRotationMatrix() *
        translationMatrix() *
        localRotationMatrix() *
        scaleMatrix();
    draw(mat, renderData, _useAcceleratedRendering);
}

void ScreenSpaceBrowser::update() {
    _objectSize = _dimensions.value();

    if (_isUrlDirty) {
        _browserInstance->loadUrl(_url);
        _isUrlDirty = false;
    }

    if (_isDimensionsDirty) {
        _browserInstance->reshape(_dimensions.value());
        _isDimensionsDirty = false;
    }
}

bool ScreenSpaceBrowser::isReady() const {
    return _shader != nullptr;
}

void ScreenSpaceBrowser::bindTexture() {
    _renderHandler->bindTexture();
}

} // namespace openspace
