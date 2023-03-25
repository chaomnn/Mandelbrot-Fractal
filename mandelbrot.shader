#version 450 core

#define LIMIT 1000

out vec4 outColor;
in vec2 point;
uniform dmat4 zoomMat;
uniform vec3 baseColor;
uniform bool julia;
uniform dvec2 constNum;

vec3 getColorSin(float iter, vec3 base) {
    iter *= 0.15;
    return 0.5 + 0.5*cos(iter + base);
}

void main() {
    dvec4 c = dvec4(double(point.x), double(point.y), 0, 1);
    c = zoomMat * c;
    dvec4 zn = dvec4(constNum.x, constNum.y, 0, 0);
    int iter = 0;

    // For Julia set mode, switch c and zn

    if (julia) {
        dvec4 temp = zn;
        zn = c;
        c = temp;
    }

    // Cardioid/period-2 bulb check

    if (!julia) {
        double m = c.x - 0.25;
        double ySqr = c.y * c.y;
        double q = m * m + ySqr;
        if (q * (q + m) <= ySqr / 4 || (c.x + 1)*(c.x + 1) + ySqr <= 0.0625) {
            outColor = vec4(0, 0, 0, 1);
            return;
        }
    }

    while (iter <= LIMIT) {

        double temp = zn.x;
        zn.x = (zn.x - zn.y) * (zn.x + zn.y) + c.x;;
        zn.y = 2.0*temp*zn.y + c.y;

        double sum = dot(zn, zn);

        if (sum > 16.0) {
            float fIter = float(iter) - log2(log2(float(sum))) + 4.0;
            outColor = vec4(getColorSin(fIter, baseColor), 1);
            break;
        } else if (iter == LIMIT) {
            outColor = vec4(0, 0, 0, 1);
        }
        ++iter;
    }
}
