using UnityEngine;
using System.Collections;
using System.Threading;

// ABSTRACT CLASS FOR JOBS
// A THREADED JOB IS HANDLED BY A WORKING THREAD
public abstract class IThreadedJob
{
    // job is done -> set true
    private bool isDone = false;
    // handle for locking
    private object handle = new object();

    // PROPERTIES
    // return if job is done
    public bool IsDone
    {
        get
        {
            bool tmp;
            lock (handle)
            {
                tmp = isDone;
            }
            return tmp;
        }
    }

    // override by derived class
    protected abstract void ThreadFunction();
    
    // called by working thread
    public void Run()
    {
        // call thread function of the derived class
        ThreadFunction();
        // mark this job as done
        isDone = true;
    }
}
