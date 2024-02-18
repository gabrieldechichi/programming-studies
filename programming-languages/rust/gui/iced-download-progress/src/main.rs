use iced::widget::text;
use iced::{executor, window, Application, Command, Settings, Size, Theme};

fn main() -> iced::Result {
    App::run(Settings {
        window: window::Settings {
            position: window::Position::Centered,
            size: Size {
                width: 400.,
                height: 250.,
            },
            decorations: false,
            exit_on_close_request: true,
            ..Default::default()
        },
        ..Default::default()
    })
}

#[derive(Debug, Clone)]
enum Message {}

struct App {}

impl Application for App {
    type Executor = executor::Default;
    type Message = Message;
    type Theme = Theme;
    type Flags = ();

    fn new(_flags: Self::Flags) -> (Self, iced::Command<Self::Message>) {
        (Self {}, Command::none())
    }

    fn title(&self) -> String {
        "Suck Up Installer".to_owned()
    }

    fn update(&mut self, message: Self::Message) -> iced::Command<Self::Message> {
        match message {}
    }

    fn view(&self) -> iced::Element<'_, Self::Message, Self::Theme, iced::Renderer> {
        text("hello world").into()
    }
}
