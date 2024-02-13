use bevy::prelude::{self, *};

use common::transport::{Event, EventHandle};

pub struct Plugin {
    pub handle: EventHandle,
}

impl prelude::Plugin for Plugin {
    fn build(&self, app: &mut AppBuilder) {
        app.add_resource(self.handle.clone())
            .add_event::<Send>()
            .add_event::<Receive>()
            .add_system(send.system())
            .add_system(receive.system())
            .add_system(log.system());
    }
}

#[derive(Debug)]
pub struct Send(Event);
#[derive(Debug)]
pub struct Receive(Event);

fn receive(handle: ResMut<EventHandle>, mut events: ResMut<Events<Receive>>) {
    if let Ok(ev) = handle.receiver.try_recv() {
        events.send(Receive(ev));
    }
}

fn send(
    handle: ResMut<EventHandle>,
    events: ResMut<Events<Send>>,
    mut reader: Local<EventReader<Send>>,
) {
    for ev in reader.iter(&events) {
        if let Err(e) = handle.sender.try_send(ev.0.clone()) {
            error!("Error sending event: {:?}", e);
        }
    }
}

fn log(events: Res<Events<Receive>>, mut reader: Local<EventReader<Receive>>) {
    for ev in reader.iter(&events) {
        info!("Event: {:?}", ev);
    }
}
