use async_std::channel::{self, Sender};
use yew::prelude::*;
use yewtil::future::LinkFuture;

use common::transport::{Event, EventHandle};

pub enum Msg {
    Send(Event),
    Receive(Event),
}

pub struct App {
    props: Props,
    link: ComponentLink<Self>,
}

#[derive(Properties, Clone)]
pub struct Props {
    handle: EventHandle,
}

impl Component for App {
    type Message = Msg;
    type Properties = Props;

    fn create(props: Self::Properties, link: ComponentLink<Self>) -> Self {
        let this = Self { props, link };
        this.listen();

        this
    }

    fn update(&mut self, msg: Self::Message) -> ShouldRender {
        match msg {
            Msg::Send(e) => {
                self.props.handle.sender.try_send(e).ok();
            }
            Msg::Receive(e) => {
                log::info!("Event: {:?}", e);
                self.listen();
            }
        }

        false
    }

    fn change(&mut self, _props: Self::Properties) -> ShouldRender {
        false
    }

    fn view(&self) -> Html {
        html! {}
    }
}

impl App {
    fn listen(&self) {
        let handle = self.props.handle.clone();
        self.link.send_future(async move {
            loop {
                match handle.receiver.recv().await {
                    Ok(msg) => return Msg::Receive(msg),
                    Err(e) => log::error!("Error receiving event: {:?}", e),
                }
            }
        });
    }
}
