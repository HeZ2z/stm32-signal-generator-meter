#include "display/lcd_font.h"

#include <stddef.h>

#include "display/lcd_prim.h"

/* 5x7 迷你点阵字库，仅覆盖当前项目需要的字符子集。 */
static const uint8_t *glyph_for_char(char ch) {
  static const uint8_t blank[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t dash[5]  = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t plus[5]  = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static const uint8_t period[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t percent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};
  static const uint8_t equal[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
  static const uint8_t zero[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t one[5]   = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t two[5]   = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t three[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t four[5]  = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t five[5]  = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t six[5]   = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t seven[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t eight[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t nine[5]  = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t A[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  static const uint8_t B[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t D[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t G[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
  static const uint8_t H[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
  static const uint8_t I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t J[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
  static const uint8_t K[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
  static const uint8_t L[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
  static const uint8_t N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
  static const uint8_t O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t Q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
  static const uint8_t R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t V[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
  static const uint8_t W[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};
  static const uint8_t X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
  static const uint8_t Y[5] = {0x03, 0x04, 0x78, 0x04, 0x03};
  static const uint8_t Z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};

  switch (ch) {
    case 'A': return A;
    case 'B': return B;
    case 'C': return C;
    case 'D': return D;
    case 'E': return E;
    case 'F': return F;
    case 'G': return G;
    case 'H': return H;
    case 'I': return I;
    case 'J': return J;
    case 'K': return K;
    case 'L': return L;
    case 'M': return M;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'Q': return Q;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'U': return U;
    case 'V': return V;
    case 'W': return W;
    case 'X': return X;
    case 'Y': return Y;
    case 'Z': return Z;
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case '-': return dash;
    case '+': return plus;
    case '.': return period;
    case '/': return slash;
    case ':': return colon;
    case '%': return percent;
    case '=': return equal;
    case ' ':
    default:
      return blank;
  }
}

void lcd_draw_char(uint16_t x, uint16_t y, char ch,
                   uint16_t fg, uint16_t bg, uint8_t scale) {
  const uint8_t *glyph = glyph_for_char(ch);

  for (uint8_t col = 0; col < 5; ++col) {
    for (uint8_t row = 0; row < 7; ++row) {
      uint16_t color = ((glyph[col] >> row) & 0x01U) != 0U ? fg : bg;
      lcd_fill_rect((uint16_t)(x + ((uint16_t)col * scale)),
                    (uint16_t)(y + ((uint16_t)row * scale)),
                    scale,
                    scale,
                    color);
    }
  }

  lcd_fill_rect((uint16_t)(x + (5U * scale)), y, scale, (uint16_t)(7U * scale), bg);
}
