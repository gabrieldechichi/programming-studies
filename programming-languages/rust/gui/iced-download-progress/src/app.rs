use iced::{
    alignment::{Horizontal, Vertical},
    widget::{button, column, container, row, text},
    Alignment, Application, Color, Command, Element, Length, Subscription, Theme,
};

use crate::{
    download,
    theme::{self},
    view,
};

#[derive(Debug)]
pub struct App<'a> {
    download_view: view::download::Download<u8, &'a str>,
}

#[derive(Debug, Clone)]
pub enum Message {
    Close,
    LauncherDownloadProgress(download::Progress),
}

impl<'a> Application for App<'a> {
    type Executor = iced::executor::Default;
    type Message = Message;
    type Theme = iced::theme::Theme;
    type Flags = ();

    fn new(_flags: Self::Flags) -> (Self, Command<Self::Message>) {
        let mut download_view =
            view::download::Download::new(0, "https://speed.hetzner.de/100MB.bin?");
        download_view.start();

        (Self { download_view }, Command::none())
    }

    fn title(&self) -> String {
        String::from("Installer")
    }

    fn update(&mut self, message: Self::Message) -> Command<Self::Message> {
        match message {
            Message::Close => std::process::exit(0),
            Message::LauncherDownloadProgress(progress) => self.download_view.update(progress),
        };
        Command::none()
    }

    fn theme(&self) -> Self::Theme {
        iced::theme::Theme::Dark
    }

    fn subscription(&self) -> Subscription<Self::Message> {
        self.download_view
            .subscription()
            .map(Message::LauncherDownloadProgress)
    }

    fn view(&self) -> Element<'_, Self::Message, Self::Theme, iced::Renderer> {
        const CLOSE_BTN_SIZE: f32 = 15.0;

        // let progress_bar_widget = self.download_view.view();

        let top_bar = {
            //close button
            let close_btn = button(
                text("X")
                    .horizontal_alignment(Horizontal::Center)
                    .vertical_alignment(Vertical::Center)
                    .style(Color::WHITE)
                    .size(15),
            )
            .on_press(Message::Close)
            .height(CLOSE_BTN_SIZE)
            .width(CLOSE_BTN_SIZE)
            .padding(2)
            .style(theme::button::Button::Custom(Box::new(
                theme::button::ButtonStyle {},
            )));

            row!(close_btn).padding(5)
        };

        container(top_bar).into()
    }
}
