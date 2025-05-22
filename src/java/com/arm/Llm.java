//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import java.util.List;
import java.util.concurrent.Flow;
import java.util.concurrent.SubmissionPublisher;
import java.util.concurrent.atomic.AtomicBoolean;

public class Llm extends SubmissionPublisher<String>
{
    static
    {
        try
        {
            System.loadLibrary("arm-llm-jni");
        } catch (UnsatisfiedLinkError e)
        {
            System.err.println("Llama: Failed to load library: arm-llm-jni");
            e.printStackTrace();
        }
    }

    private long llmPtr = 0;
    private String modelTag = "";
    private String userTag = "";
    private List<String> stopWords = null;
    private String cachedToken = "";
    private String emitToken = "";
    private String llmPrefix = "";
    private AtomicBoolean evaluatedOnce = new AtomicBoolean(false);
    //#ToDo move to LlmConfig
    private int numThreads = 4;
    private int batchSize = 256;

    // Native method declarations
    /**
    Method to create LlmConfig cpp instance from params.
    @param modelTag name used to refer the Model
    @param modelPath path to load model from
    @param llmPrefix Initial-prompt to load into llm before query
    @param numThreads Number of threads for inference
    @param batchSize batch size used to chunk queries
    */
    public native long createLlmConfig(String modelTag, String modelPath, String llmPrefix,
                                       int numThreads, int batchSize);
    /**
     * Method for loading LLM model
     @param pathToModel file path for loading model
     @return pointer to loaded model
     */
    public native long loadModel(long LlmConfig);

    /**
     * Method for freeing LLM model
     * @param modelPtr to free model
     */
    private native void freeLlm();

    /**
     * Public method for getting encode timing
     * @return timings in tokens/s for encoding prompt
     */
    public native float getEncodeRate();

    /**
     * Public method for getting decode timing
     * @return timings in tokens/s for decoding prompt
     */
    public native float getDecodeRate();

   /**
     * Private method for resetting conversation history
    */
   public native void resetContext();

    /**
     * Method for resetting timing information
     */
    public native void resetTimings();

    /**
     * Method to encode the given text
     * @param text     the prompt to be encoded
     */
    private native void encode(String text);

    /**
    * Method to get Next Token once encoding is done.
    * This Method needs to be called in a loop while monitoring for Stop-Words.
    * @return next Token as String
    */
    private native String getNextToken();

    /**
     * Method to get chat Progress in percentage
     */
     public native int getChatProgress();

    /**
     * Method to decode answers one by one, once prefill stage is completed
     *
     * @param nPrompts     prompt length used for benchmarking
     * @param nEvalPrompts number of generated tokens for benchmarking
     * @param nMaxSeq      sequence number
     * @param nRep         number of repetitions
     */
    public native String benchModel(
            int nPrompts,
            int nEvalPrompts,
            int nMaxSeq,
            int nRep
    );

    /**
     *Method to separate Initialization from constructor
     *@param llmConfig type configuration file to load model
     */
    public void llmInit(LlmConfig llmConfig)
    {
        this.stopWords = llmConfig.getStopWords();
        this.modelTag = llmConfig.getModelTag();
        this.userTag = llmConfig.getUserTag();
        this.llmPrefix = llmConfig.getLlmPrefix();
        this.numThreads = llmConfig.getNumThreads();
        long configPtr = createLlmConfig(this.modelTag,llmConfig.getModelPath(),
                                         this.llmPrefix,this.numThreads,this.batchSize);
        this.llmPtr = loadModel(configPtr);
    }
    public void setSubscriber(Flow.Subscriber<String> subscriber)
    {
        System.out.println("subscribed set from llama");
        this.subscribe(subscriber);
    }

    /**
     * Method to get response of a query asynchronously
     * @param Query the prompt asked
     */
    public  void  sendAsync(String Query)
    {
        String query = "";
        AtomicBoolean stop = new AtomicBoolean(false);
        if (evaluatedOnce.get())
            query = userTag + Query + modelTag;
        else
            query = llmPrefix + Query + modelTag;
        encode(query);
        evaluatedOnce.set(true);
        while (getChatProgress()<100)
        {
            String s = getNextToken();
            stop.set(inspectWord(s));
            if (stop.get())
            {
                // needed for showing end of stream, Closing publisher will result in error
                // for next query
                emitToken = "<eos>";
                this.submit(emitToken);

                break;
            }
            this.submit(emitToken);
        }
    }

    /**
     * Method to get response of a query synchronously
     * @param Query the prompt asked
     * @return response of LLM
     */
    public String send(String Query)
    {
        String response = "";
        String query = "";
        boolean stop = false;
        if (evaluatedOnce.get())
            query = userTag + Query + modelTag;
        else
            query = llmPrefix + Query + modelTag;
        encode(query);
        evaluatedOnce.set(true);
        while (getChatProgress()<100)
        {
            String s = getNextToken();
            stop = inspectWord(s);
            response += emitToken;
            if (stop)
              break;
        }
        return response;
    }

   /**
   * Method to find any stop-Words or partial stop-Word present in current token
   * @param str current token decoded
   * @return boolean for detection of stop word
   */
   private boolean inspectWord(String str)
   {
       boolean stopWordTriggered = false;
       String evaluationString = this.cachedToken + str;
       // if stopWord is in evaluationString break loop.
       for (String word : stopWords)
       {
           //use position to access inclusion of Stop-words. Preserve the substring before Stop word.
           int position = evaluationString.indexOf(word);
           if(position!=-1)
           {
               stopWordTriggered = true;
               emitToken = evaluationString.substring(0,position);
               cachedToken = "";
               return stopWordTriggered;
           }
       }
       emitToken = evaluationString;
       for (String word : stopWords)
       {
           String partialWord = word;
           partialWord = partialWord.substring(0, partialWord.length() - 1);
           while (!partialWord.isEmpty())
           {
               // if the beginning for stop word coincides with end of emitted token don't emit current token.
               if (evaluationString.endsWith(partialWord))
               {
                   emitToken = "";
                   break;
               } else
               {
                   partialWord = partialWord.substring(0, partialWord.length() - 1);
               }
           }
       }
       this.cachedToken = emitToken.isEmpty() ? evaluationString : "";
       return stopWordTriggered;
   }

    /**
    * Sets the LLM prefix used for query processing.
    * @param llmPrefix initial prompt for llm
    */
    public void setLlmPrefix(String llmPrefix)
    {
      this.llmPrefix = llmPrefix;
    }

    /**
    * Sets the LLM ModelTag
    */
    public void setLlmModelTag(String newTag)
    {
       this.modelTag = newTag;
    }
    /**
     * Method to free model from memory
     */
    public void freeModel()
    {
        freeLlm();
        this.close();
        evaluatedOnce.set(false);
    }


}
