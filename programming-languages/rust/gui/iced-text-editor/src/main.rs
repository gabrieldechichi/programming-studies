use std::io;
use std::path::{Path, PathBuf};
use std::process::exit;
use std::sync::Arc;

use iced::keyboard::KeyCode;
use iced::widget::{column, container, horizontal_space, row, text, text_editor};
use iced::{executor, keyboard, subscription, Application, Command, Length, Settings, Theme};

fn main() -> iced::Result {
    Editor::run(Settings::default())
}

struct Editor {
    content: text_editor::Content,
}

#[derive(Debug, Clone)]
enum Message {
    Edit(text_editor::Action),
    FileOpened(Result<Arc<String>, io::ErrorKind>),
    Exit,
}

impl Application for Editor {
    type Message = Message;
    type Executor = executor::Default;
    type Theme = Theme;
    type Flags = ();

    fn new(_f: Self::Flags) -> (Self, Command<Message>) {
        (
            Self {
                content: text_editor::Content::new(),
            },
            Command::perform(
                load_file(format!("{}/src/main.rs", env!("CARGO_MANIFEST_DIR"))),
                Message::FileOpened,
            ),
        )
    }

    fn title(&self) -> String {
        "Text Editor".to_owned()
    }

    fn update(&mut self, message: Self::Message) -> Command<Message> {
        match message {
            Message::Edit(action) => self.content.edit(action),
            Message::FileOpened(r) => {
                if let Ok(file_contents) = r {
                    self.content = text_editor::Content::with(&file_contents);
                }
            }
            Message::Exit => exit(0),
        }
        Command::none()
    }

    fn view(&self) -> iced::Element<'_, Self::Message> {
        let input = text_editor(&self.content).on_edit(Message::Edit);

        let status_bar = {
            let (line, column) = self.content.cursor_position();
            let cursor_pos_text = text(format!("{}:{}", line, column));
            row![horizontal_space(Length::Fill), cursor_pos_text]
        };

        container(column![input, status_bar].spacing(10))
            .padding(10)
            .into()
    }

    fn theme(&self) -> iced::Theme {
        iced::Theme::Dark
    }

    fn subscription(&self) -> iced::Subscription<Self::Message> {
        keyboard::on_key_press(|key, _modifier| {
            match key {
                KeyCode::Escape => Some(Message::Exit),
                _ =>None
            }
        })
    }
}

async fn load_file(path: impl AsRef<Path>) -> Result<Arc<String>, io::ErrorKind> {
    tokio::fs::read_to_string(path)
        .await
        .map(Arc::new)
        .map_err(|e| e.kind())
}
