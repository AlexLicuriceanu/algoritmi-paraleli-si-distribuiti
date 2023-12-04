/* Implement this class. */

import java.util.List;

public class MyDispatcher extends Dispatcher {

    private int lastHostIndex = -1;

    public MyDispatcher(SchedulingAlgorithm algorithm, List<Host> hosts) {
        super(algorithm, hosts);
    }

    @Override
    public void addTask(Task task) {

        if (this.algorithm == SchedulingAlgorithm.ROUND_ROBIN) {
            roundRobin(task);
        }
        else if (this.algorithm == SchedulingAlgorithm.SHORTEST_QUEUE) {
            shortestQueue(task);
        }
        else if (this.algorithm == SchedulingAlgorithm.SIZE_INTERVAL_TASK_ASSIGNMENT) {
            sizeIntervalTaskAssignment(task);
        }
        else if (this.algorithm == SchedulingAlgorithm.LEAST_WORK_LEFT) {
            leastWorkLeft(task);
        }
    }

    private void roundRobin(Task task) {
        if (this.hosts.isEmpty()) {
            return;
        }

        synchronized (this) {
            lastHostIndex = (lastHostIndex + 1) % this.hosts.size();

            Host host = this.hosts.get(lastHostIndex);
            host.addTask(task);
        }
    }

    private void shortestQueue(Task task) {
        if (this.hosts.isEmpty()) {
            return;
        }

        synchronized (this) {

            Host minHost = this.hosts.get(0);
            int minQueueSize = minHost.getQueueSize();

            for (Host host : this.hosts) {
                if (host.getQueueSize() < minQueueSize) {

                    minHost = host;
                    minQueueSize = host.getQueueSize();

                }
                else if (host.getQueueSize() == minQueueSize) {

                    if (host.getId() < minHost.getId()) {
                        minHost = host;
                        minQueueSize = host.getQueueSize();
                    }
                }
            }

            minHost.addTask(task);
        }
    }

    private void sizeIntervalTaskAssignment(Task task) {
        synchronized (this) {

            int hostIndex = task.getType().ordinal();

            if (hostIndex + 1 > this.hosts.size()) {
                return;
            }

            Host host = this.hosts.get(hostIndex);
            host.addTask(task);
        }
    }

    private void leastWorkLeft(Task task) {
        if (this.hosts.isEmpty()) {
            return;
        }

        synchronized (this) {

            Host minWorkHost = this.hosts.get(0);
            long minWorkLeft = minWorkHost.getWorkLeft();

            for (Host host : this.hosts) {
                if (host.getWorkLeft() < minWorkLeft) {

                    minWorkLeft = host.getWorkLeft();
                    minWorkHost = host;
                }
                else if (host.getWorkLeft() == minWorkLeft) {

                    if (host.getId() < minWorkHost.getId()) {
                        minWorkHost = host;
                        minWorkLeft = host.getWorkLeft();
                    }
                }
            }

            minWorkHost.addTask(task);
        }
    }
}
