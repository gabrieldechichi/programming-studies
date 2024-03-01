pub mod download {
    use iced::Element;
    use iced::{
        widget::{column, progress_bar, text},
        Length,
    };
    use iced::alignment::{Alignment, Horizontal, Vertical};
    use iced::{Color, Subscription};

    use crate::download::{self, Progress};
    use std::hash::Hash;

    #[derive(Debug)]
    pub enum State {
        Idle,
        Downloading { progress: f32 },
        Finished,
        Errored,
    }

    #[derive(Debug)]
    pub struct Download<I, U>
    where
        U: ToString,
        I: 'static + Hash + Clone,
    {
        pub id: I,
        pub url: U,
        pub state: State,
    }

    impl<I, U> Download<I, U>
    where
        U: ToString,
        I: 'static + Hash + Copy + Send + Sync,
    {
        pub fn new(id: I, url: U) -> Self {
            Self {
                id,
                url,
                state: State::Idle,
            }
        }

        pub fn start(&mut self) {
            match self.state {
                State::Idle | State::Finished | State::Errored => {
                    self.state = State::Downloading { progress: 0.0 }
                }
                State::Downloading { progress: _ } => {}
            }
        }

        pub fn subscription(&self) -> Subscription<Progress> {
            match self.state {
                State::Downloading { .. } => download::file(self.id, &self.url),
                _ => Subscription::none(),
            }
        }

        pub fn update(&mut self, progress: download::Progress) {
            match progress {
                Progress::Started => self.state = State::Downloading { progress: 0.0 },
                Progress::Advanced(p) => self.state = State::Downloading { progress: p },
                Progress::Finished => self.state = State::Finished,
                Progress::Errored => self.state = State::Errored,
            }
        }

        pub fn view<'a, Message: 'a>(&'a self) -> Element<Message> {
            let progress = match self.state {
                State::Idle => 0.0,
                State::Downloading { progress } => progress * 100.0,
                State::Finished => 100.0,
                State::Errored => 0.0,
            };
            let progress_text = match self.state {
                State::Idle => "".to_owned(),
                State::Downloading { .. } => format!("Downloading {:.1}%", progress),
                State::Finished => "Success".to_owned(),
                State::Errored => "Error!".to_owned(),
            };

            column!(
                //progress text
                text(progress_text)
                    .style(iced::theme::Text::Color(Color::WHITE))
                    .size(18)
                    .horizontal_alignment(Horizontal::Center)
                    .vertical_alignment(Vertical::Center),
                //progress bar
                progress_bar(0.0..=100.0, progress)
                    .width(Length::Fill)
                    .height(6),
            )
            .align_items(Alignment::Start)
            .height(Length::Shrink)
            .spacing(5)
            .width(Length::Fill)
            .into()
        }
    }
}
