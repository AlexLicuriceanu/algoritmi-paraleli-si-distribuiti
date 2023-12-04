import java.util.Comparator;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.PriorityBlockingQueue;

public class MyHost extends Host {

    // Priority queue to store the node's tasks, ordered by priority.
    private final BlockingQueue<Task> taskQueue =
            new PriorityBlockingQueue<>(1, Comparator
                    .comparingInt(Task::getPriority).reversed()
                    );

    private Task runningTask = null;
    private volatile boolean running = true;
    private volatile boolean execute = true;

    @Override
    public void run() {
        while (running) {
            if (!taskQueue.isEmpty()) {
                Task task = taskQueue.poll();

                runningTask = task;
                executeTask(task);
            }
        }
    }

    private void executeTask(Task task) {
        long duration = task.getLeft();
        long startTime = System.currentTimeMillis();

        while (task.getLeft() > 0 && execute) {

            // Compute elapsed time since last iteration.
            long elapsedTime = System.currentTimeMillis() - startTime;

            // Make remainingTime 0 if negative.
            long remainingTime = Math.max(0, duration - elapsedTime);

            // Update the Task with the remaining time.
            task.setLeft(remainingTime);

            // Time is up, mark task as finished and set the running task to null.
            if (task.getLeft() == 0) {
                task.finish();
                runningTask = null;
            }

            // Host signaled to stop the execution of the current task, reset execute bool
            // and exit this function.
            if (!execute) {
                execute = true;
                return;
            }
        }
    }

    @Override
    public void addTask(Task task) {
        taskQueue.add(task);

        if (runningTask != null && runningTask.isPreemptible()) {
            if (task.getPriority() > runningTask.getPriority()) {

                // If the running task is preemptible and has a lower priority than the new task,
                // set execute = false, to signal the execution loop to stop the current task.
                execute = false;
                taskQueue.add(runningTask);
            }
        }
    }


    @Override
    public int getQueueSize() {
        return taskQueue.size() + ((runningTask == null) ? 0 : 1);
    }


    @Override
    public long getWorkLeft() {
        long workLeft = 0;
        for (Task task : taskQueue) {
            workLeft += task.getLeft();
        }

        if (runningTask != null) {
            workLeft += runningTask.getLeft();
        }

        return workLeft;
    }

    @Override
    public void shutdown() {
        running = false;
    }

}