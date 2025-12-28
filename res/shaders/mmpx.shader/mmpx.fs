// Copyright 2020 Morgan McGuire & Mara Gagiu, 
// provided under the Open Source MIT license https://opensource.org/licenses/MIT

varying vec2 texCoord;
uniform sampler2D tex;
uniform vec2 texSize;

float luma(vec4 C) {
    return (C.r + C.g + C.b + 1.) * (1. - C.a);
}

bool all_eq2(vec4 B, vec4 A0, vec4 A1) {
    return all(bvec2(B == A0, B == A1));
}

bool all_eq3(vec4 B, vec4 A0, vec4 A1, vec4 A2) {
    return all(bvec3(B == A0, B == A1, B == A2));
}

bool all_eq4(vec4 B, vec4 A0, vec4 A1, vec4 A2, vec4 A3) {
    return all(bvec4(B == A0, B == A1, B == A2, B == A3));
}

bool any_eq2(vec4 B, vec4 A0, vec4 A1) {
    return any(bvec2(B == A0, B == A1));
}

bool any_eq3(vec4 B, vec4 A0, vec4 A1, vec4 A2) {
    return any(bvec3(B == A0, B == A1, B == A2));
}

bool any_eq4(vec4 B, vec4 A0, vec4 A1, vec4 A2, vec4 A3) {
    return any(bvec4(B == A0, B == A1, B == A2, B == A3));
}

vec4 src(vec2 offset) {
    vec2 coord = texCoord + offset / texSize;
    return texture2D(tex, coord);
}

void main() {
    vec4 A = src(vec2(-1., -1.));
    vec4 B = src(vec2( 0., -1.));
    vec4 C = src(vec2(+1., -1.));

    vec4 D = src(vec2(-1.,  0.));
    vec4 E = src(vec2( 0.,  0.));
    vec4 F = src(vec2(+1.,  0.));

    vec4 G = src(vec2(-1., +1.));
    vec4 H = src(vec2( 0., +1.));
    vec4 I = src(vec2(+1., +1.));

    vec4 J = E;
    vec4 K = E;
    vec4 L = E;
    vec4 M = E;

    bvec3 top = bvec3(A == E, B == E, C == E);
    bvec3 mid = bvec3(D == E, true,   F == E);
    bvec3 bot = bvec3(G == E, H == E, I == E);

    if (!all(top) || !all(mid) || !all(bot)) {
        vec4 P = src(vec2(0.  -2.));
        vec4 S = src(vec2(0.  +2.));
        vec4 Q = src(vec2(-2., 0.));
        vec4 R = src(vec2(+2., 0.));
        float Bl = luma(B);
        float Dl = luma(D);
        float El = luma(E);
        float Fl = luma(F);
        float Hl = luma(H);

        //     P
        //   A B C
        // Q D E F R
        //   G H I
        //     R

        // 1:1 slope rules
        if (
            (D == B && D != H && D != F) &&
            (El >= Dl || E == A) &&
            any_eq3(E, A, C, G) &&
            ((El < Dl) || A != D || E != P || E != Q)
        ) J = D;
        if (
            (B == F && B != D && B != H) &&
            (El >= Bl || E == C) &&
            any_eq3(E, A, C, I) &&
            ((El < Bl) || C != B || E != P || E != R)
        ) K = B;
        if (
            (H == D && H != F && H != B) &&
            (El >= Hl || E == G) &&
            any_eq3(E, A, G, I) &&
            ((El < Hl) || G != H || E != S || E != Q)
        ) L = H;
        if (
            (F == H && F != B && F != D) &&
            (El >= Fl || E == I) &&
            any_eq3(E, C, G, I) &&
            ((El < Fl) || I != H || E != R || E != S)
        ) M = F;

        // Intersection rules
        if (
            E != F &&
            all_eq4(E, C, I, D, Q) &&
            all_eq2(F, B, H) &&
            F != src(vec2(+3.,  0.))
        ) K = M = F;
        if (
            E != D &&
            all_eq4(E, A, G, F, R) &&
            all_eq2(D, B, H) &&
            D != src(vec2(-3.,  0.))
        ) J = L = D;
        if (
            E != H &&
            all_eq4(E, G, I, B, P) &&
            all_eq2(H, D, F) &&
            H != src(vec2( 0., +3.))
        ) L = M = H;
        if (
            E != B &&
            all_eq4(E, A, C, H, S) &&
            all_eq2(B, D, F) &&
            B != src(vec2( 0., -3.))
        ) J = K = B;

        if (
            Bl < El &&
            all_eq4(E, G, H, I, S) &&
            !any_eq4(E, A, D, C, F)
        ) J = K = B;
        if (
            Hl < El &&
            all_eq4(E, A, B, C, P) &&
            !any_eq4(E, D, G, I, F)
        ) L = M = H;
        if (
            Fl < El &&
            all_eq4(E, A, D, G, Q) &&
            !any_eq4(E, B, C, I, H)
        ) K = M = F;
        if (
            Dl < El &&
            all_eq4(E, C, F, I, R) &&
            !any_eq4(E, B, A, G, H)
        ) J = L = D;

        // 2:1 slope rules
        if (H != B) { 
            if (H != A && H != E && H != C) {
                if (all_eq3(H, G, F, R) && !any_eq2(H, D, src(vec2(+2., -1.)))) L = M;
                if (all_eq3(H, I, D, Q) && !any_eq2(H, F, src(vec2(-2., -1.)))) M = L;
            }
            
            if (B != I && B != G && B != E) {
                if (all_eq3(B, A, F, R) && !any_eq2(B, D, src(vec2(+2., +1.)))) J = K;
                if (all_eq3(B, C, D, Q) && !any_eq2(B, F, src(vec2(-2., +1.)))) K = J;
            }
        } // H !== B
        
        if (F != D) { 
            if (D != I && D != E && D != C) {
                if (all_eq3(D, A, H, S) && !any_eq2(D, B, src(vec2(+1., +2.)))) J = L;
                if (all_eq3(D, G, B, P) && !any_eq2(D, H, src(vec2(+1., -2.)))) L = J;
            }
            
            if (F != E && F != A && F != G) {    
                if (all_eq3(F, C, H, S) && !any_eq2(F, B, src(vec2(-1., +2.)))) K = M;
                if (all_eq3(F, I, B, P) && !any_eq2(F, H, src(vec2(-1., -2.)))) M = K;
            }
        } // F !== D
    } // not constant

    if (fract((texCoord * texSize).x) < 0.5) {
        if (fract((texCoord * texSize).y) < 0.5) {
            gl_FragColor = J;
        } else {
            gl_FragColor = L;
        }
    } else {
        if (fract((texCoord * texSize).y) < 0.5) {
            gl_FragColor = K;
        } else {
            gl_FragColor = M;
        }
    }
}
