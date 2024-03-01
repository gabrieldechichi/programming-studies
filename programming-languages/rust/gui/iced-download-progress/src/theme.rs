pub mod button {
    use iced::widget::button::Appearance;
    use iced::widget::button::StyleSheet;
    use iced::Color;

    pub type Button = iced::theme::Button;
    pub struct ButtonStyle;

    impl StyleSheet for ButtonStyle {
        type Style = iced::Theme;

        fn active(&self, _style: &Self::Style) -> Appearance {
            Appearance {
                background: Some(Color::TRANSPARENT.into()),
                text_color: Color::WHITE,
                ..Default::default()
            }
        }

        fn hovered(&self, style: &Self::Style) -> Appearance {
            self.active(style)
        }

        fn pressed(&self, style: &Self::Style) -> Appearance {
            self.active(style)
        }
    }
}
