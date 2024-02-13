import { makeScene2D } from "@motion-canvas/2d/lib/scenes";
import { createRef } from "@motion-canvas/core/lib/utils";
import {
    CodeBlock,
    edit,
    insert,
    lines,
    remove,
} from "@motion-canvas/2d/lib/components/CodeBlock";
import { all, waitFor } from "@motion-canvas/core/lib/flow";

export default makeScene2D(function* (view) {
    const codeRef = createRef<CodeBlock>();
    const customCodeTheme = {
        stringContent: { text: "#B8BB26" }, // Yellowish for string contents
        stringPunctuation: { text: "#FB4934" }, // Reddish for string punctuation
        variable: { text: "#83A598" }, // Blue for variables/members like `size`
        parameter: { text: "#8EC07C" }, // Green for parameters
        comment: { text: "#928374" }, // Greyish-brown for comments
        regexpContent: { text: "#B8BB26" }, // Yellowish for Regular Expression content (similar to stringContent)
        literal: { text: "#D3869B" }, // Pinkish for literals
        keyword: { text: "#FB4934" }, // Reddish for keywords
        entityName: { text: "#FABD2F" }, // Yellowish for entity names (types like Paddle, Ball, Vec2)
    };

    // Note: Again, adjustments may be needed based on the actual behavior and capabilities of the system you're using.

    // Note: If the library or framework you're using has

    // #[derive(Component)]
    // struct Paddle;

    // #[derive(Component)]
    // struct Ball {
    //     size: Vec2,
    // }
    yield view.add(
        <CodeBlock
            ref={codeRef}
            language="rust"
            theme={customCodeTheme}
            code={`
    `}
        ></CodeBlock>
    );

    yield* codeRef().edit(0.8, false)`
${insert(`#[derive(Component)]`)}
    `;

    yield* codeRef().edit(0.8, false)`
#[derive(Component)]
${insert(`struct Paddle;`)}
    `;

    // second line only
    yield* codeRef().selection(lines(1), 1);
    yield* waitFor(1);
});
