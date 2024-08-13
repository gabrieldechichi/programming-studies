RaymarchResult eyeDeadpool(vec3 p, float mult) {
    RaymarchResult r;

    //start 6
    p.xy += vec2(-0.55 * mult, -0.1);
    p.x /= 1.2;
    p.y /= 1.1;

    float eye = sdCylinderZ(p, 0.15, 0.2);

    r.distance = eye;
    r.color = vec3(1.0);
    //end 6


    //start 7
    p.xy += vec2(0.22 * mult, -0.2);
    eye = opSubtraction(eye, sdCylinderZ(p, 0.72, 0.3));

    r.distance = eye;
    r.color = vec3(1.0);
    //end 7

    return r;
}

RaymarchResult raymarchDeadpool(vec3 p) {
    RaymarchResult r;

    //start 2
    p = rotateY(p, iTime);
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);

    r.distance = outerCylinder;
    r.color = vec3(0.7, 0.1, 0.1);
    //end 2

    //start 3
    p = rotateY(p, iTime);
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);
    float outerCylinderHole = sdCylinderZ(p, 0.4, 1.0);
    outerCylinder = opSubtraction(outerCylinder, outerCylinderHole);

    r.distance = outerCylinder;
    r.color = vec3(0.7, 0.1, 0.1);
    //end 3

    //start 4
    p = rotateY(p, iTime);
    float outerCylinder = sdCylinderZ(p, 0.15, 1.2);
    float outerCylinderHole = sdCylinderZ(p, 0.4, 1.0);
    outerCylinder = opSubtraction(outerCylinder, outerCylinderHole);

    float innerCylinder = sdCylinderZ(p, 0.10, 1.0);

    if (outerCylinder < innerCylinder) {
        r.distance = outerCylinder;
        r.color = vec3(0.7, 0.1, 0.1);
    } else {
        r.color = vec3(0.12);
        r.distance = innerCylinder;
    }
    //end 4

    //start 5
    RaymarchResult eyeRight = eyeDeadpool(p, 1.);
    RaymarchResult eyeLeft = eyeDeadpool(p, -1.);

    if (eyeRight.distance < r.distance) {
        r = eyeRight;
    }
    if (eyeLeft.distance < r.distance) {
        r = eyeLeft;
    }
    //end 5


    //start 8
    float outerBox = sdBox(p, vec3(0.15, 1.20 - 0.01, 0.15));
    outerCylinder = opUnion(outerBox, outerCylinder);
    //end 8

    return r;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (2.0 * fragCoord - iResolution.xy)
            / iResolution.x;

    vec3 ro = vec3(0., 0., -3.5);
    vec3 rd = normalize(vec3(uv.x, uv.y, 1.));

    vec3 color = vec3(uv, 1.);
    float t = MAX_RM_DISTANCE;

    //start 1
    RaymarchResult lDeadpool = logoDeadpool(ro, rd);
    color = lDeadpool.color;
    t = lDeadpool.distance;

    color *= 2. / t;
    //end 1

    fragColor = vec4(color, 1.);
}
