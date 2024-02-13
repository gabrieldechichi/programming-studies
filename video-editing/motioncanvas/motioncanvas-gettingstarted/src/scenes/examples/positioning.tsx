import { Circle, Node, makeScene2D, Grid, Line } from "@motion-canvas/2d";
import { createRef, Vector2 } from "@motion-canvas/core";

const RED = "#ff6470";
const GREEN = "#99C47A";
const BLUE = "#68ABDF";

export default makeScene2D(function* (view) {
    const group = createRef<Node>();
    view.add(
        <Node ref={group} x={-100}>
            <Grid
                width={view.width() * 2}
                height={view.height() * 2}
                spacing={60}
                stroke={"#444"}
                lineWidth={1}
                lineCap="square"
                cache
            />
            <Circle
                width={120}
                height={120}
                stroke={BLUE}
                lineWidth={4}
                startAngle={110}
                endAngle={340}
            />
            <Line
                stroke={RED}
                lineWidth={4}
                endArrow
                arrowSize={10}
                points={[
                    [0, 0],
                    [70, 0],
                ]}
            />
            <Line
                stroke={GREEN}
                lineWidth={4}
                endArrow
                arrowSize={10}
                points={[
                    [0, 0],
                    [0, 70],
                ]}
            />
            <Circle width={20} height={20} fill="#fff" />
        </Node>
    );

    yield* group().position.x(100, 0.8);
    yield* group().rotation(30, 0.8);
    yield* group().scale(2, 0.8);
    yield* group().position.x(-100, 1.0);
    yield* group().rotation(0.0, 0.8);
    yield* group().scale(1, 0.8);
});
