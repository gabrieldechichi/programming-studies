use yew::prelude::*;

#[function_component]
fn App() -> Html {
    let node_ref = NodeRef::default();

    html! {
        <>
            <div class="bg-red-400">
                <canvas ref={node_ref.clone()} />
            </div>
        </>
    }
}

fn main() {
    yew::Renderer::<App>::new().render();
}
