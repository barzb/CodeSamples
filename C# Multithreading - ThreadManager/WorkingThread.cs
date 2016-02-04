using UnityEngine;
using System.Collections;
using System.Threading;

// A WORKING THREAD CAN DO JOBS
public class WorkingThread
{
    // systen thread
    private System.Threading.Thread thread = null;
    // true: abort thread in next frame
    private bool abort;
    // job queue
    private Queue jobs = new Queue();
    // handle for locking
    private object handle = new object();

    // PROPERTIES
    // get count of jobs in queue
    public int JobsLeft
    {
        get
        {
            int tmp;
            lock (handle)
            {
                tmp = jobs.Count;
            }
            return tmp;
        }
    }
    // returns if the thread is alive or dead
    public bool isAlive { get { return (!abort && thread.IsAlive); } }


    // init this thread
    public void Initialize()
    {
        abort = false;
        thread = new System.Threading.Thread(ThreadFunction);
        thread.IsBackground = true;
        thread.Start();
        Debug.Log("START THREAD " + thread.ManagedThreadId + " @" + Time.realtimeSinceStartup);
    }

    // add a job to the job queue
    public void EnqueueJob(IThreadedJob job)
    {
        if (job != null) {
            jobs.Enqueue(job);
        }
    }
    
    // will be executed by system thread
    private void ThreadFunction()
    {
        // run as long as thread isn't aborted
        while(thread.IsAlive && !abort)
        {
            CompleteJobs();
        }

        Debug.Log("CLOSE THREAD " + thread.ManagedThreadId);

        // abort system thread
        thread.Abort();
        // garbage collector will collect the homeless object soon
        thread = null;
        // clear job queue
        jobs.Clear();
        World.threadManager.CleanUp();
    }

    // main method of the working thread
    private void CompleteJobs()
    {
        if (jobs.Count == 0)
            return;
        
        // loop through all the jobs in the queue 
        while (jobs.Count != 0)
        {
            // stop if thread is aborted from outside the thread
            if (abort)
                return;

            // get first job in queue and work it
            IThreadedJob currentJob = jobs.Dequeue() as IThreadedJob;
            currentJob.Run();
        }

        // abort thread if queue is empty
        abort = true;
    }


    // can be called from outside this thread to abort this thread
    public void Abort()
    {
        Debug.Log("ABORT THREAD " + thread.ManagedThreadId + " @" + Time.realtimeSinceStartup);
        // thread is marked "aborted" and system thread will be stopped after 
        // the currently executed job is done
        abort = true;
    }
}
