#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>   
#include <imgui_impl_opengl3.h> 
#include "ComplexParser.h" 
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>

#define PI 3.14159265358979323846

// 全局控制状态变量
double centerX = 0.0, centerY = 0.0;
float zoom = 150.0f;
int colorMode = 0;
int showContours = 1;
int cartesianStyle = 0;
int useBlueSign = 1;

char formulaInput[256] = "exp(1/z)";
bool isValidFormula = true;

// 探测器实时值本地冻结缓存
double cached_z_re = 0.0, cached_z_im = 0.0;
double cached_fz_re = 0.0, cached_fz_im = 0.0;

ComplexParser parserCore;
GLuint shaderProgram = 0;
GLuint vertexShader = 0;

bool isDragging = false;
double lastMouseX = 0.0, lastMouseY = 0.0;

// 全局定义右侧独立画板Canvas的物理尺寸与坐标参数
float canvasX = 455.0f;
float canvasY = 5.0f;
float canvasWidth = 0.0f;
float canvasHeight = 0.0f;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    if (mx >= canvasX && mx <= canvasX + canvasWidth && my >= canvasY && my <= canvasY + canvasHeight) {
        if (yoffset > 0) zoom *= 1.1f;
        else zoom *= 0.9f;
        zoom = std::max(10.0f, std::min(zoom, 10000.0f));
    }
}

bool rebuildFragmentShader(const std::string& glslExpr) {
    std::string fragTemplatePath = std::string(SHADER_DIR) + "complex.frag";
    std::ifstream fFile(fragTemplatePath);
    if (!fFile.is_open()) return false;

    std::stringstream buffer;
    buffer << fFile.rdbuf();
    std::string shaderCode = buffer.str();

    std::string target = "USER_EXPRESSION_PLACEHOLDER";
    size_t pos = shaderCode.find(target);
    if (pos != std::string::npos) {
        shaderCode.replace(pos, target.length(), glslExpr + ";");
    }

    const char* src = shaderCode.c_str();
    GLuint newFragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(newFragShader, 1, &src, nullptr);
    glCompileShader(newFragShader);

    int success;
    glGetShaderiv(newFragShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(newFragShader);
        return false;
    }

    GLuint newProgram = glCreateProgram();
    glAttachShader(newProgram, vertexShader);
    glAttachShader(newProgram, newFragShader);
    glLinkProgram(newProgram);

    glGetProgramiv(newProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteShader(newFragShader);
        glDeleteProgram(newProgram);
        return false;
    }

    if (shaderProgram != 0) glDeleteProgram(shaderProgram);
    shaderProgram = newProgram;
    glDeleteShader(newFragShader);
    return true;
}

GLuint compileVertexShader(const std::string& filePath) {
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open()) return 0;
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderCode = shaderStream.str();
    const char* src = shaderCode.c_str();
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

void SetupTailwindSlateStyle() {
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.ScaleAllSizes(1.5f);

    colors[ImGuiCol_WindowBg] = ImVec4(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f); // 纯白窗体底色
    colors[ImGuiCol_ChildBg] = ImVec4(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f); // Slate-300 框边
    colors[ImGuiCol_FrameBg] = ImVec4(248.0f / 255.0f, 250.0f / 255.0f, 252.0f / 255.0f, 1.0f); // 输入框自适应极浅灰

    colors[ImGuiCol_Text] = ImVec4(0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f);       // 纯黑文本
    colors[ImGuiCol_TextDisabled] = ImVec4(71.0f / 255.0f, 85.0f / 255.0f, 105.0f / 255.0f, 1.0f);   // 强对比深灰

    colors[ImGuiCol_Button] = ImVec4(67.0f / 255.0f, 56.0f / 255.0f, 202.0f / 255.0f, 1.0f);   // Indigo-700 经典深靛蓝
    colors[ImGuiCol_ButtonHovered] = ImVec4(79.0f / 255.0f, 70.0f / 255.0f, 229.0f / 255.0f, 1.0f);   // Indigo-600 悬浮高亮蓝
    colors[ImGuiCol_ButtonActive] = ImVec4(55.0f / 255.0f, 48.0f / 255.0f, 163.0f / 255.0f, 1.0f);   // Indigo-800 按下深靛蓝

    colors[ImGuiCol_TitleBg] = ImVec4(241.0f / 255.0f, 245.0f / 255.0f, 249.0f / 255.0f, 1.0f); // Title浅石板灰
    colors[ImGuiCol_TitleBgActive] = ImVec4(226.0f / 255.0f, 232.0f / 255.0f, 240.0f / 255.0f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(67.0f / 255.0f, 56.0f / 255.0f, 202.0f / 255.0f, 1.0f);   // 钩钩颜色同步为靛蓝
    colors[ImGuiCol_FrameBgHovered] = ImVec4(226.0f / 255.0f, 232.0f / 255.0f, 240.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(203.0f / 255.0f, 213.0f / 255.0f, 225.0f / 255.0f, 1.0f);
}

void AddTextWithBorderOutline(ImDrawList* drawList, const ImVec2& pos, const char* text) {
    ImU32 blackOutline = IM_COL32(0, 0, 0, 240);
    ImU32 whiteCenter = IM_COL32(255, 255, 255, 255);
    drawList->AddText(ImVec2(pos.x - 1.0f, pos.y), blackOutline, text);
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y), blackOutline, text);
    drawList->AddText(ImVec2(pos.x, pos.y - 1.0f), blackOutline, text);
    drawList->AddText(ImVec2(pos.x, pos.y + 1.0f), blackOutline, text);
    drawList->AddText(pos, whiteCenter, text);
}

void RenderCoordinateAxes() {
    ImDrawList* bgDrawList = ImGui::GetBackgroundDrawList();

    bgDrawList->AddRect(ImVec2(canvasX, canvasY), ImVec2(canvasX + canvasWidth, canvasY + canvasHeight), IM_COL32(148, 163, 184, 255), 0.0f, 0, 2.0f);

    float axisX = canvasX + canvasWidth * 0.5f - (float)centerX * zoom;
    float axisY = canvasY + canvasHeight * 0.5f + (float)centerY * zoom;

    if (axisY >= canvasY && axisY <= canvasY + canvasHeight) {
        bgDrawList->AddLine(ImVec2(canvasX, axisY), ImVec2(canvasX + canvasWidth, axisY), IM_COL32(100, 116, 139, 160), 2.0f);
    }
    if (axisX >= canvasX && axisX <= canvasX + canvasWidth) {
        bgDrawList->AddLine(ImVec2(axisX, canvasY), ImVec2(axisX, canvasY + canvasHeight), IM_COL32(100, 116, 139, 160), 2.0f);
    }

    double minRe = centerX - (canvasWidth * 0.5) / zoom;
    double maxRe = centerX + (canvasWidth * 0.5) / zoom;
    double minIm = centerY - (canvasHeight * 0.5) / zoom;
    double maxIm = centerY + (canvasHeight * 0.5) / zoom;

    double step = 1.0;
    if (zoom < 45.0f)  step = 5.0;
    if (zoom < 12.0f)  step = 20.0;
    if (zoom < 3.0f)   step = 100.0;
    if (zoom > 400.0f) step = 0.5;
    if (zoom > 1500.0f) step = 0.1;

    double startRe = std::floor(minRe / step) * step;
    for (double r = startRe; r <= maxRe; r += step) {
        float sx = canvasX + canvasWidth * 0.5f + (float)(r - centerX) * zoom;
        if (sx < canvasX || sx > canvasX + canvasWidth) continue;

        bgDrawList->AddLine(ImVec2(sx, canvasY), ImVec2(sx, canvasY + canvasHeight), IM_COL32(71, 85, 105, 35), 1.0f);

        char label[32];
        if (step >= 1.0) sprintf_s(label, "%.0f", r);
        else sprintf_s(label, "%.1f", r);

        float sy = axisY + 8.0f;
        if (sy < canvasY + 5.0f) sy = canvasY + 5.0f;
        if (sy > canvasY + canvasHeight - 35.0f) sy = canvasY + canvasHeight - 35.0f;

        if (std::abs(r) > 1e-6) {
            AddTextWithBorderOutline(bgDrawList, ImVec2(sx + 5.0f, sy), label);
        }
    }

    double startIm = std::floor(minIm / step) * step;
    for (double i_val = startIm; i_val <= maxIm; i_val += step) {
        float sy = canvasY + canvasHeight * 0.5f - (float)(i_val - centerY) * zoom;
        if (sy < canvasY || sy > canvasY + canvasHeight) continue;

        bgDrawList->AddLine(ImVec2(canvasX, sy), ImVec2(canvasX + canvasWidth, sy), IM_COL32(71, 85, 105, 35), 1.0f);

        char label[32];
        if (std::abs(i_val) < 1e-6) {
            sprintf_s(label, "0");
        }
        else {
            if (step >= 1.0) sprintf_s(label, "%.0fi", i_val);
            else sprintf_s(label, "%.1fi", i_val);
        }

        float sx = axisX + 8.0f;
        if (sx < canvasX + 5.0f) sx = canvasX + 5.0f;
        if (sx > canvasX + canvasWidth - 85.0f) sx = canvasX + canvasWidth - 85.0f;

        if (std::abs(i_val) > 1e-6) {
            AddTextWithBorderOutline(bgDrawList, ImVec2(sx, sy - 15.0f), label);
        }
        else {
            AddTextWithBorderOutline(bgDrawList, ImVec2(axisX - 25.0f, axisY + 8.0f), "0");
        }
    }
}

void DrawQuadrantIndicatorTable(ImDrawList* drawList, const ImVec2& startPos, float width) {
    float cellW = width * 0.5f;
    float cellH = 40.0f;

    drawList->AddRectFilled(startPos, ImVec2(startPos.x + cellW, startPos.y + cellH), IM_COL32(0, 0, 120, 255), 4.0f, ImDrawFlags_RoundCornersTopLeft);
    drawList->AddRectFilled(ImVec2(startPos.x + cellW, startPos.y), ImVec2(startPos.x + width, startPos.y + cellH), IM_COL32(0, 0, 0, 255), 4.0f, ImDrawFlags_RoundCornersTopRight);
    drawList->AddRectFilled(ImVec2(startPos.x, startPos.y + cellH), ImVec2(startPos.x + cellW, startPos.y + cellH * 2), IM_COL32(0, 0, 200, 255), 4.0f, ImDrawFlags_RoundCornersBottomLeft);
    drawList->AddRectFilled(ImVec2(startPos.x + cellW, startPos.y + cellH), ImVec2(startPos.x + width, startPos.y + cellH * 2), IM_COL32(0, 0, 255, 255), 4.0f, ImDrawFlags_RoundCornersBottomRight);

    drawList->AddLine(ImVec2(startPos.x + cellW, startPos.y), ImVec2(startPos.x + cellW, startPos.y + cellH * 2), IM_COL32(255, 255, 255, 60), 1.5f);
    drawList->AddLine(ImVec2(startPos.x, startPos.y + cellH), ImVec2(startPos.x + width, startPos.y + cellH), IM_COL32(255, 255, 255, 60), 1.5f);

    // 强对比文字标识覆盖
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
    ImGui::SetCursorScreenPos(ImVec2(startPos.x + 25, startPos.y + 6)); ImGui::Text("Re<0, Im>0");
    ImGui::SetCursorScreenPos(ImVec2(startPos.x + 25, startPos.y + cellH + 6)); ImGui::Text("Re<0, Im<0");
    ImGui::SetCursorScreenPos(ImVec2(startPos.x + cellW + 25, startPos.y + cellH + 6)); ImGui::Text("Re>0, Im<0");
    ImGui::SetCursorScreenPos(ImVec2(startPos.x + cellW + 25, startPos.y + 6)); ImGui::Text("Re>0, Im>0");
    ImGui::PopStyleColor();
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 960, "复变函数可视化计算器", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetWindowSizeLimits(window, 1600, 960, GLFW_DONT_CARE, GLFW_DONT_CARE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    SetupTailwindSlateStyle();

    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 26.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());

    std::string vertPath = std::string(SHADER_DIR) + "complex.vert";
    vertexShader = compileVertexShader(vertPath);

    parserCore.parse(formulaInput);
    rebuildFragmentShader(parserCore.generateGLSL());

    float vertices[] = {
        -1.0f,  1.0f,  -1.0f, -1.0f,   1.0f, -1.0f,
        -1.0f,  1.0f,   1.0f, -1.0f,   1.0f,  1.0f
    };
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    std::vector<std::string> presets = {
        "z",          "exp(1/z)",   "ln(z^2)",
        "sin(z)",     "atan(z)",    "cosh(z)"
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        canvasWidth = (float)width - canvasX - 5.0f;
        canvasHeight = (float)height - canvasY - 5.0f;

        bool isMouseInCanvas = (mouseX >= canvasX && mouseX <= canvasX + canvasWidth &&
            mouseY >= canvasY && mouseY <= canvasY + canvasHeight);
        bool shouldUpdateDetector = isMouseInCanvas && (!io.WantCaptureMouse) && glfwGetWindowAttrib(window, GLFW_HOVERED);

        if (shouldUpdateDetector) {
            double canvasCenterX = canvasX + canvasWidth * 0.5;
            double canvasCenterY = canvasY + canvasHeight * 0.5;
            cached_z_re = (mouseX - canvasCenterX) / zoom + centerX;
            cached_z_im = -(mouseY - canvasCenterY) / zoom + centerY;

            std::complex<double> z_input(cached_z_re, cached_z_im);
            std::complex<double> fz_output = parserCore.evaluate(z_input);
            cached_fz_re = fz_output.real();
            cached_fz_im = fz_output.imag();
        }

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging) {
                if (!io.WantCaptureMouse && isMouseInCanvas) {
                    isDragging = true;
                    lastMouseX = mouseX;
                    lastMouseY = mouseY;
                }
            }
            else {
                double dx = mouseX - lastMouseX;
                double dy = mouseY - lastMouseY;
                centerX -= dx / zoom;
                centerY += dy / zoom;
                lastMouseX = mouseX;
                lastMouseY = mouseY;
            }
        }
        else {
            isDragging = false;
        }

        glViewport(0, 0, width, height);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        float sideBarWidth = 445.0f;

        // ====================
        ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sideBarWidth, 200), ImGuiCond_Always);
        ImGui::Begin("函数配置与预设", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        ImGui::Text("输入函数表达式 f(z) =");
        ImGui::SetNextItemWidth(-1);

        if (!isValidFormula) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(254.0f / 255.0f, 226.0f / 255.0f, 226.0f / 255.0f, 1.0f));
        else ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(241.0f / 255.0f, 245.0f / 255.0f, 249.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

        if (ImGui::InputText("##FormulaInput", formulaInput, IM_ARRAYSIZE(formulaInput))) {
            if (parserCore.parse(formulaInput)) isValidFormula = rebuildFragmentShader(parserCore.generateGLSL());
            else isValidFormula = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        for (size_t i = 0; i < presets.size(); i++) {
            if (ImGui::Button(presets[i].c_str(), ImVec2(132, 32))) {
                strcpy_s(formulaInput, presets[i].c_str());
                if (parserCore.parse(formulaInput)) isValidFormula = rebuildFragmentShader(parserCore.generateGLSL());
            }
            if ((i + 1) % 3 != 0 && i < presets.size() - 1) ImGui::SameLine();
        }
        ImGui::PopStyleColor();
        ImGui::End();

        // ====================
        ImGui::SetNextWindowPos(ImVec2(5, 210), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sideBarWidth, 125), ImGuiCond_Always);
        ImGui::Begin("指针位置", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        double z_r = std::sqrt(cached_z_re * cached_z_re + cached_z_im * cached_z_im);
        double z_pi_angle = std::atan2(cached_z_im, cached_z_re) / PI;
        double fz_r = std::sqrt(cached_fz_re * cached_fz_re + cached_fz_im * cached_fz_im);
        double fz_pi_angle = std::atan2(cached_fz_im, cached_fz_re) / PI;

        ImGui::Text("z  =");
        ImGui::SameLine(100); ImGui::TextColored(ImVec4(29.0f / 255.0f, 78.0f / 255.0f, 216.0f / 255.0f, 1.0f), "%.2f %s %.2fi", cached_z_re, (cached_z_im >= 0 ? "+" : "-"), std::abs(cached_z_im));
        ImGui::SameLine(300); ImGui::TextColored(ImVec4(29.0f / 255.0f, 78.0f / 255.0f, 216.0f / 255.0f, 0.85f), "%.2f ∠ %.2fπ", z_r, z_pi_angle);

        ImGui::Separator();

        ImGui::Text("f(z) =");
        ImGui::SameLine(100); ImGui::TextColored(ImVec4(4.0f / 255.0f, 120.0f / 255.0f, 87.0f / 255.0f, 1.0f), "%.2f %s %.2fi", cached_fz_re, (cached_fz_im >= 0 ? "+" : "-"), std::abs(cached_fz_im));
        ImGui::SameLine(300); ImGui::TextColored(ImVec4(4.0f / 255.0f, 120.0f / 255.0f, 87.0f / 255.0f, 0.85f), "%.2f ∠ %.2fπ", fz_r, fz_pi_angle);
        ImGui::End();

        // ====================
        ImGui::SetNextWindowPos(ImVec2(5, 340), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sideBarWidth, 265), ImGuiCond_Always);
        ImGui::Begin("渲染模式", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        if (ImGui::RadioButton("极坐标", &colorMode, 0)) {}
        ImGui::SameLine(180);
        if (ImGui::RadioButton("直角坐标", &colorMode, 1)) {}

        ImGui::Spacing(); ImGui::Separator();
        if (colorMode == 0) {
            ImGui::Text("极坐标参数:");
            bool contoursBool = (showContours == 1);
            if (ImGui::Checkbox("显示模等高线", &contoursBool)) showContours = contoursBool ? 1 : 0;
        }
        else {
            ImGui::Text("直角坐标参数:");
            ImGui::RadioButton("正弦波", &cartesianStyle, 0); ImGui::SameLine(180);
            ImGui::RadioButton("线性折叠", &cartesianStyle, 1);
            bool blueSignBool = (useBlueSign == 1);
            if (ImGui::Checkbox("启用蓝色象限指示分量", &blueSignBool)) useBlueSign = blueSignBool ? 1 : 0;
        }
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("重置视图", ImVec2(-1, 36))) {
            centerX = 0.0; centerY = 0.0; zoom = 150.0f;
        }
        ImGui::PopStyleColor();
        ImGui::End();

        // ====================
        ImGui::SetNextWindowPos(ImVec2(5, 610), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sideBarWidth, 345), ImGuiCond_Always);

        std::string legendTitle = (colorMode == 0) ? "图例 (极坐标)" : "图例 (直角坐标)";
        ImGui::Begin(legendTitle.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImDrawList* legendDrawList = ImGui::GetWindowDrawList();
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        float availW = ImGui::GetContentRegionAvail().x;

        if (colorMode == 0) {
            ImGui::Text("明暗 = 模长");

            ImVec2 barPos = ImGui::GetCursorScreenPos();
            float barH = 26.0f;
            for (int x = 0; x < (int)(availW - 20.0f); x++) {
                float r = ((float)x / (availW - 20.0f)) * 5.0f;
                float l_val = 0.5f * (1.0f - 1.0f / (1.0f + std::pow(r, 0.3f))) * 2.0f;
                if (showContours == 1) {
                    float logR = std::log(r + 1e-9f);
                    float contour = 0.5f + 0.5f * std::sin(10.0f * logR);
                    l_val = 0.3f + 0.4f * std::pow(r / (r + 1.0f), 0.2f) * contour;
                }
                ImU32 grey = IM_COL32(l_val * 255, l_val * 255, l_val * 255, 255);
                legendDrawList->AddLine(ImVec2(barPos.x + x, barPos.y), ImVec2(barPos.x + x, barPos.y + barH), grey);
            }

            for (int k = 0; k <= 5; k++) {
                float kx = barPos.x + (k / 5.0f) * (availW - 20.0f);
                legendDrawList->AddLine(ImVec2(kx, barPos.y), ImVec2(kx, barPos.y + barH), IM_COL32(180, 50, 50, 200), 1.5f);
            }
            ImGui::Dummy(ImVec2(availW, barH + 5.0f));
            ImGui::Text("0"); ImGui::SameLine(availW * 0.2f); ImGui::Text("1"); ImGui::SameLine(availW * 0.39f);
            ImGui::Text("2"); ImGui::SameLine(availW * 0.59f); ImGui::Text("3"); ImGui::SameLine(availW * 0.78f);
            ImGui::Text("4"); ImGui::SameLine(availW * 0.95f); ImGui::Text("5+");

            float wheelOffsetY = 135.0f;
            float wheelSize = 120.0f;
            ImVec2 wCenter = ImVec2(startPos.x + availW - wheelSize * 0.5f - 40.0f, startPos.y + wheelOffsetY + wheelSize * 0.5f + 5.0f);
            float wRadius = wheelSize * 0.45f;

            for (int a = 0; a < 360; a += 3) {
                float r1 = (float)(a * PI / 180.0); float r2 = (float)((a + 3) * PI / 180.0);
                ImVec2 p1 = ImVec2(wCenter.x + std::cos(r1) * wRadius, wCenter.y - std::sin(r1) * wRadius);
                ImVec2 p2 = ImVec2(wCenter.x + std::cos(r2) * wRadius, wCenter.y - std::sin(r2) * wRadius);
                float rv, gv, bv; ImGui::ColorConvertHSVtoRGB((float)a / 360.0f, 1.0f, 1.0f, rv, gv, bv);
                legendDrawList->AddTriangleFilled(wCenter, p1, p2, ImGui::ColorConvertFloat4ToU32(ImVec4(rv, gv, bv, 1.0f)));
            }
            legendDrawList->AddCircle(wCenter, wRadius, IM_COL32(148, 163, 184, 255), 0, 1.5f);

            ImGui::SetCursorScreenPos(ImVec2(wCenter.x + wRadius + 4, wCenter.y - 12)); ImGui::Text("0");
            ImGui::SetCursorScreenPos(ImVec2(wCenter.x - 18, wCenter.y - wRadius - 26)); ImGui::Text("π/2");
            ImGui::SetCursorScreenPos(ImVec2(wCenter.x - wRadius - 22, wCenter.y - 12)); ImGui::Text("π");
            ImGui::SetCursorScreenPos(ImVec2(wCenter.x - 26, wCenter.y + wRadius + 4)); ImGui::Text("-π/2");

            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10.0f, startPos.y + wheelOffsetY + 0.0f)); ImGui::Text("色相 = 辐角");
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10.0f, startPos.y + wheelOffsetY + 30.0f)); ImGui::Text("红: 0 (1+)");
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10.0f, startPos.y + wheelOffsetY + 60.0f)); ImGui::Text("青: π (1-)");
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10.0f, startPos.y + wheelOffsetY + 90.0f)); ImGui::Text("绿: π/2 (i+)");
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10.0f, startPos.y + wheelOffsetY + 120.0f)); ImGui::Text("紫: -π/2 (i-)");
        }
        else {
            float previewH = 75.0f;
            if (cartesianStyle == 0) {
                ImGui::BulletText("R = 255 · [1 - sin(|π·Re(f(z))|)] / 2");
                ImGui::BulletText("G = 255 · [1 - sin(|π·Im(f(z))|)] / 2");

                ImVec2 gridPos = ImGui::GetCursorScreenPos();
                for (int y = 0; y < (int)previewH; y += 2) {
                    for (int x = 0; x < (int)availW; x += 2) {
                        float u = ((float)x / availW) * 8.0f - 4.0f;
                        float v = ((float)y / previewH) * 8.0f - 4.0f;
                        float r = 255.0f * (1.0f - std::sin(std::abs(u * PI))) / 2.0f;
                        float g = 255.0f * (1.0f - std::sin(std::abs(v * PI))) / 2.0f;
                        legendDrawList->AddRectFilled(ImVec2(gridPos.x + x, gridPos.y + y), ImVec2(gridPos.x + x + 2, gridPos.y + y + 2), IM_COL32(r, g, 0, 255));
                    }
                }
                AddTextWithBorderOutline(legendDrawList, ImVec2(gridPos.x + 10, gridPos.y + 5), "Re -> Red");
                AddTextWithBorderOutline(legendDrawList, ImVec2(gridPos.x + 10, gridPos.y + previewH - 30), "Im -> Green");
            }
            else {
                ImGui::BulletText("R = |128 · Re(f(z))| mod 256");
                ImGui::BulletText("G = |128 · Im(f(z))| mod 256");

                ImVec2 gridPos = ImGui::GetCursorScreenPos();
                for (int y = 0; y < (int)previewH; y += 2) {
                    for (int x = 0; x < (int)availW; x += 2) {
                        float u = ((float)x / availW) * 8.0f - 4.0f;
                        float v = ((float)y / previewH) * 8.0f - 4.0f;
                        float r = std::fmod(std::abs(u * 128.0f), 256.0f);
                        float g = std::fmod(std::abs(v * 128.0f), 256.0f);
                        legendDrawList->AddRectFilled(ImVec2(gridPos.x + x, gridPos.y + y), ImVec2(gridPos.x + x + 2, gridPos.y + y + 2), IM_COL32(r, g, 0, 255));
                    }
                }
                AddTextWithBorderOutline(legendDrawList, ImVec2(gridPos.x + 10, gridPos.y + 5), "Re -> Red");
                AddTextWithBorderOutline(legendDrawList, ImVec2(gridPos.x + 10, gridPos.y + previewH - 30), "Im -> Green");
            }
            ImGui::Dummy(ImVec2(availW, previewH + 10.0f));

            if (useBlueSign == 1) {
                ImGui::BulletText("B = ");
                ImVec2 tableStart = ImGui::GetCursorScreenPos();
                DrawQuadrantIndicatorTable(legendDrawList, tableStart, availW);
            }
        }
        ImGui::End();

        RenderCoordinateAxes();

        GLint gl_canvasX = static_cast<GLint>(canvasX);
        GLint gl_canvasY = static_cast<GLint>(5);
        GLsizei gl_canvasW = static_cast<GLsizei>(canvasWidth);
        GLsizei gl_canvasH = static_cast<GLsizei>(canvasHeight);
        glViewport(gl_canvasX, gl_canvasY, gl_canvasW, gl_canvasH);

        glUseProgram(shaderProgram);

        glUniform2f(glGetUniformLocation(shaderProgram, "u_center"), (float)centerX, (float)centerY);
        glUniform1f(glGetUniformLocation(shaderProgram, "u_zoom"), zoom);
        glUniform2f(glGetUniformLocation(shaderProgram, "u_screenSize"), canvasWidth, canvasHeight);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_colorMode"), colorMode);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_showContours"), showContours);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_cartesianStyle"), cartesianStyle);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_useBlueSign"), useBlueSign);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glViewport(0, 0, width, height);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO); glDeleteProgram(shaderProgram); glDeleteShader(vertexShader);
    glfwTerminate();
    return 0;
}