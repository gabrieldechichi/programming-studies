// import { Circle, makeScene2D, View2D } from "@motion-canvas/2d";
// import {
//     all,
//     createRef,
//     makeRef,
//     ThreadGenerator,
//     waitFor,
// } from "@motion-canvas/core";

// export default makeScene2D(function* (view) {
//     // Create your animations here
//     const myCircle = createRef<Circle>();

//     view.add(
//         <Circle
//             ref={myCircle}
//             x={-300}
//             width={140}
//             height={140}
//             fill="#e13238"
//         />
//     );
//     yield* flicker(myCircle());
// });

// function* sideToSide(myCircle: Circle): ThreadGenerator {
//     yield* all(
//         myCircle.position.x(300, 1).to(-300, 1),
//         myCircle.fill("#e6a700", 1).to("#e13238", 1)
//     );
// }

// function* flicker(circle: Circle): ThreadGenerator {
//     yield* circle.fill("red", 1);
//     yield* circle.fill("blue", 1);
//     yield* circle.fill("red", 1);
// }

import { Circle, makeScene2D, Rect, View2D } from "@motion-canvas/2d";
import {
    all,
    createRef,
    makeRef,
    range,
    ThreadGenerator,
    waitFor,
} from "@motion-canvas/core";

export default makeScene2D(function* (view) {
    const rects: Rect[] = [];
    view.add(
        range(2).map((i) => (
            <Rect
                ref={makeRef(rects, i)}
                width={100}
                height={100}
                x={-250 + 125 * i}
                fill="#88C0D0"
                radius={10}
            />
        ))
    );
    yield* waitFor(1);

    // Animate them
    yield* all(
        ...rects.map((rect) => rect.position.y(100, 1).to(-100, 2).to(0, 1))
    );
});
