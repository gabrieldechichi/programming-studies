use std::hash::Hash;

#[derive(Debug)]
enum State {
    Ready {
        url: String,
    },
    Downloading {
        response: reqwest::Response,
        total: u64,
        downloaded: u64,
    },
    Success,
    Errored,
}

#[derive(Debug, Clone)]
pub enum Progress {
    Started,
    Advanced(f32),
    Finished,
    Errored,
}

pub fn file<I, T>(id: I, url: &T) -> iced::Subscription<Progress>
where
    I: 'static + Hash + Copy + Send + Sync,
    T: ToString,
{
    iced::subscription::unfold(
        id,
        State::Ready {
            url: url.to_string(),
        },
        move |state| async {
            let next_state = update_download(state).await;
            let progress = match next_state {
                State::Ready { url: _ } => Progress::Started,
                State::Downloading {
                    response: _,
                    total,
                    downloaded,
                } => Progress::Advanced(downloaded as f32 / total as f32),
                State::Success => Progress::Finished,
                State::Errored => Progress::Errored,
            };
            (progress, next_state)
        },
    )
}

async fn update_download(state: State) -> State {
    match state {
        State::Ready { url } => {
            let response = reqwest::get(&url).await;
            match response {
                Ok(response) => {
                    if let Some(total) = response.content_length() {
                        State::Downloading {
                            response,
                            total,
                            downloaded: 0,
                        }
                    } else {
                        State::Errored
                    }
                }
                Err(_) => todo!(),
            }
        }
        State::Downloading {
            mut response,
            total,
            mut downloaded,
        } => match response.chunk().await {
            Ok(Some(chunk)) => {
                downloaded = downloaded + chunk.len() as u64;
                State::Downloading {
                    response,
                    total,
                    downloaded,
                }
            }
            Ok(None) => State::Success,
            Err(_) => State::Errored,
        },
        State::Success => iced::futures::future::pending().await,
        State::Errored => iced::futures::future::pending().await,
    }
}
