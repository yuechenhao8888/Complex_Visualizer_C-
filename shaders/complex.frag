#version 330 core
out vec4 FragColor;
in vec2 TexCoords; 

uniform vec2 u_center;       
uniform float u_zoom;        
uniform vec2 u_screenSize;   
uniform int u_colorMode;
uniform int u_showContours;  
uniform int u_cartesianStyle;
uniform int u_useBlueSign;   

#define PI 3.14159265359

// 计算两个复数的加法
vec2 complex_add(vec2 c1, vec2 c2) { 
    return c1 + c2;
}

// 计算两个复数的减法
vec2 complex_sub(vec2 c1, vec2 c2) { 
    return c1 - c2;
}

// 计算两个复数的乘法
vec2 complex_mul(vec2 c1, vec2 c2) { 
    return vec2(c1.x * c2.x - c1.y * c2.y, c1.x * c2.y + c1.y * c2.x);
}

// 计算两个复数的除法
vec2 complex_div(vec2 c1, vec2 c2) {
    float denom = c2.x * c2.x + c2.y * c2.y;
    return vec2(c1.x * c2.x + c1.y * c2.y, c1.y * c2.x - c1.x * c2.y) / denom;
}

// 计算复数的指数函数
vec2 complex_exp(vec2 c) { 
    return exp(c.x) * vec2(cos(c.y), sin(c.y));
}

// 计算复数的自然对数函数
vec2 complex_log(vec2 c) { 
    return vec2(log(length(c) + 1e-9), atan(c.y, c.x)); 
}

// 计算复数的常用对数函数
vec2 complex_log10(vec2 c) { 
    return complex_log(c) / log(10.0);
}

// 计算复数的幂函数
vec2 complex_pow(vec2 c, float p) {
    float r = pow(length(c) + 1e-9, p);
    float t = atan(c.y, c.x) * p;
    return vec2(r * cos(t), r * sin(t));
}

// 计算复数的平方根函数
vec2 complex_sqrt(vec2 c) {
    float r = length(c);
    float re = sqrt(max(0.0, (r + c.x) * 0.5));
    float im = sqrt(max(0.0, (r - c.x) * 0.5));
    if (c.y < 0.0) im = -im;
    return vec2(re, im);
}

// 计算复数的正弦函数
vec2 complex_sin(vec2 c) { 
    return vec2(sin(c.x) * cosh(c.y), cos(c.x) * sinh(c.y));
}

// 计算复数的余弦函数
vec2 complex_cos(vec2 c) { 
    return vec2(cos(c.x) * cosh(c.y), -sin(c.x) * sinh(c.y)); 
}

// 计算复数的正切函数
vec2 complex_tan(vec2 c) { 
    return complex_div(complex_sin(c), complex_cos(c));
}

// 计算复数的双曲正弦函数
vec2 complex_sinh(vec2 c) { 
    return vec2(sinh(c.x) * cos(c.y), cosh(c.x) * sin(c.y));
}

// 计算复数的双曲余弦函数
vec2 complex_cosh(vec2 c) { 
    return vec2(cosh(c.x) * cos(c.y), sinh(c.x) * sin(c.y)); 
}

// 计算复数的双曲正切函数
vec2 complex_tanh(vec2 c) { 
    return complex_div(complex_sinh(c), complex_cosh(c));
}

// 计算复数的反正弦函数
vec2 complex_asin(vec2 c) {
    vec2 z2 = complex_mul(c, c);
    vec2 one_minus_z2 = complex_sub(vec2(1.0, 0.0), z2);
    vec2 sqrt_one_minus_z2 = complex_sqrt(one_minus_z2);
    vec2 iz = vec2(-c.y, c.x);
    vec2 sum = complex_add(iz, sqrt_one_minus_z2);
    vec2 log_sum = complex_log(sum);
    return vec2(log_sum.y, -log_sum.x);
}

// 计算复数的反余弦函数
vec2 complex_acos(vec2 c) {
    return complex_sub(vec2(PI * 0.5, 0.0), complex_asin(c));
}

// 计算复数的反正切函数
vec2 complex_atan(vec2 c) {
    vec2 i_plus_z = complex_add(vec2(0.0, 1.0), c);
    vec2 i_minus_z = complex_sub(vec2(0.0, 1.0), c);
    vec2 log1 = complex_log(i_plus_z);
    vec2 log2 = complex_log(i_minus_z);
    vec2 diff = complex_sub(log1, log2);
    diff.y = mod(diff.y + PI, 2.0 * PI) - PI;
    return vec2(-diff.y * 0.5, diff.x * 0.5);
}

// 动态解析用户输入的复变函数表达式
vec2 evaluate_fz(vec2 z) {
    return USER_EXPRESSION_PLACEHOLDER;
}

// 将色相角度转换为红绿蓝颜色分量
float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

// 将格式化的色彩空间转换为红绿蓝颜色向量
vec3 hslToRgb(float h, float s, float l) {
    if (s == 0.0) return vec3(l);
    float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    float p = 2.0 * l - q;
    float r = hue2rgb(p, q, h / 360.0 + 1.0/3.0);
    float g = hue2rgb(p, q, h / 360.0);
    float b = hue2rgb(p, q, h / 360.0 - 1.0/3.0);
    return vec3(r, g, b);
}

// 程序主渲染管线函数
void main() {
    vec2 absCoord = TexCoords * u_screenSize;
    vec2 cx = u_screenSize * 0.5;
    float re = (absCoord.x - cx.x) / u_zoom + u_center.x;
    float im = (absCoord.y - cx.y) / u_zoom + u_center.y; 
    vec2 z = vec2(re, im);
    vec2 fz = evaluate_fz(z);
    vec3 finalColor = vec3(0.0);
    if (u_colorMode == 0) {
        float r = length(fz);
        float theta = atan(fz.y, fz.x);
        float hue = theta * 180.0 / PI;
        if (hue < 0.0) hue += 360.0;
        float l = 0.5;
        if (u_showContours == 1) {
            float logR = log(r + 1e-9);
            float contour = 0.5 + 0.5 * sin(10.0 * logR);
            l = 0.3 + 0.4 * pow(r / (r + 1.0), 0.2) * contour;
        } else {
            l = 0.5 * (1.0 - 1.0 / (1.0 + pow(r, 0.3))) * 2.0;
            if (l > 0.9) l = 0.9;
        }
        finalColor = hslToRgb(hue, 1.0, l);
    } else {
        float a = fz.x;
        float b = fz.y;
        float rawR, rawG, rawB;
        if (u_cartesianStyle == 0) {
            rawR = 255.0 * (1.0 - sin(abs(a * PI))) / 2.0;
            rawG = 255.0 * (1.0 - sin(abs(b * PI))) / 2.0;
        } else {
            rawR = mod(abs(a * 128.0), 256.0);
            rawG = mod(abs(b * 128.0), 256.0);
        }
        if (u_useBlueSign == 1) {
            if (a >= 0.0 && b >= 0.0)       rawB = 0.0;
            else if (a < 0.0 && b >= 0.0)  rawB = 120.0;
            else if (a < 0.0 && b < 0.0)   rawB = 200.0;
            else                            rawB = 255.0;
        } else {
            rawB = 0.0;
        }
        finalColor = vec3(rawR / 255.0, rawG / 255.0, rawB / 255.0);
    }
    FragColor = vec4(finalColor, 1.0);
}