const std = @import("std");

const Semaphore = std.Thread.Semaphore;
const Thread = std.Thread;

const ThreadInfo = struct { //
    id: u16,
    queue: *WorkerQueue,
};

const WorkerEntry = struct { msg: []const u8 };

const WorkerQueue = struct { //
    buffer: [num_work]WorkerEntry,
    entry_count: u16,
    next_entry: u16,
    completed_count: u16,
    semaphore: *Semaphore,
};

fn queueWork(queue: *WorkerQueue, entry: WorkerEntry) void {
    const entry_count = @atomicLoad(u16, &queue.entry_count, .SeqCst);
    queue.buffer[entry_count] = entry;
    @atomicStore(u16, &queue.entry_count, entry_count + 1, .SeqCst);
    queue.semaphore.post();
}

fn doWork(thread_info: *ThreadInfo) void {
    var queue = thread_info.queue;

    while (true) {
        const entry_count = @atomicLoad(u16, &queue.entry_count, .SeqCst);
        const next_entry = @atomicLoad(u16, &queue.next_entry, .SeqCst);
        if (next_entry < entry_count) {
            @atomicStore(u16, &queue.next_entry, next_entry + 1, .SeqCst);
            const entry = queue.buffer[next_entry];
            std.debug.print("Thread {}: {s}\n\n", .{ thread_info.id, entry.msg });
            //sleep for 100ms
            std.time.sleep(1*1000_000);
            _ = @atomicRmw(u16, &queue.completed_count, .Add, 1, .SeqCst);
        } else {
            queue.semaphore.wait();
        }
    }
}

const num_work = 100;
const n_threads = 16;

pub fn main() !void {
    var semaphore = Semaphore{};
    var worker_queue: WorkerQueue = WorkerQueue{ //
        .buffer = undefined,
        .entry_count = 0,
        .next_entry = 0,
        .completed_count = 0,
        .semaphore = &semaphore,
    };

    var infos: [n_threads]ThreadInfo = undefined;
    for (0..n_threads) |i| {
        infos[i] = ThreadInfo{ //
            .id = @intCast(i),
            .queue = &worker_queue,
        };
        const handle = try Thread.spawn(.{}, doWork, .{&infos[i]});
        handle.detach();
    }

    for (0..num_work) |_| {
        queueWork(&worker_queue, WorkerEntry{ .msg = "A" ++ "i" });
    }

    var timer = try std.time.Timer.start();
    while (@atomicLoad(u16, &worker_queue.completed_count, .SeqCst) < @atomicLoad(u16, &worker_queue.entry_count, .SeqCst)) {}
    const duration = timer.lap();
    std.debug.print("Work took: {d:2}ms", .{duration / 1_000_000});
}
