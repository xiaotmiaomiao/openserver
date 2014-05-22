/* 
 * File:   _private.h
 * Author: root
 *
 * Created on May 4, 2014, 8:08 PM
 */

#ifndef _PRIVATE_H
#define	_PRIVATE_H

#ifdef	__cplusplus
extern "C" {
#endif

//int load_modules(unsigned int);		/*!< Provided by loader.c */
//int load_pbx(void);			/*!< Provided by pbx.c */
int init_logger(void);			/*!< Provided by logger.c */
void close_logger(void);		/*!< Provided by logger.c */
//int init_framer(void);			/*!< Provided by frame.c */
int css_term_init(void);		/*!< Provided by term.c */
//int cssdb_init(void);			/*!< Provided by db.c */
//void css_channels_init(void);		/*!< Provided by channel.c */
void css_builtins_init(void);		/*!< Provided by cli.c */
int css_cli_perms_init(int reload);	/*!< Provided by cli.c */
//int dnsmgr_init(void);			/*!< Provided by dnsmgr.c */ 
//void dnsmgr_start_refresh(void);	/*!< Provided by dnsmgr.c */
//int dnsmgr_reload(void);		/*!< Provided by dnsmgr.c */
void threadstorage_init(void);		/*!< Provided by threadstorage.c */
//int css_event_init(void);		/*!< Provided by event.c */
//int css_device_state_engine_init(void);	/*!< Provided by devicestate.c */
int cssobj2_init(void);			/*!< Provided by cssobj2.c */
//int css_file_init(void);		/*!< Provided by file.c */
//int css_features_init(void);            /*!< Provided by features.c */
//void css_autoservice_init(void);	/*!< Provided by autoservice.c */
//int css_data_init(void);		/*!< Provided by data.c */
//int css_http_init(void);		/*!< Provided by http.c */
//int css_http_reload(void);		/*!< Provided by http.c */
//int css_tps_init(void); 		/*!< Provided by taskprocessor.c */
//int css_timing_init(void);		/*!< Provided by timing.c */
//int css_indications_init(void); /*!< Provided by indications.c */
//int css_indications_reload(void);/*!< Provided by indications.c */
//void css_stun_init(void);               /*!< Provided by stun.c */
//int css_cel_engine_init(void);		/*!< Provided by cel.c */
//int css_cel_engine_reload(void);	/*!< Provided by cel.c */
//int css_ssl_init(void);                 /*!< Provided by ssl.c */
//int css_test_init(void);            /*!< Provided by test.c */
/*!
 * \brief Reload ceictims modules.
 * \param name the name of the module to reload
 *
 * This function reloads the specified module, or if no modules are specified,
 * it will reload all loaded modules.
 *
 * \note Modules are reloaded using their reload() functions, not unloading
 * them and loading them again.
 * 
 * \return 0 if the specified module was not found.
 * \retval 1 if the module was found but cannot be reloaded.
 * \retval -1 if a reload operation is already in progress.
 * \retval 2 if the specfied module was found and reloaded.
 */
//int css_module_reload(const char *name);

/*!
 * \brief Process reload requests received during startup.
 *
 * This function requests that the loader execute the pending reload requests
 * that were queued during server startup.
 *
 * \note This function will do nothing if the server has not completely started
 *       up.  Once called, the reload queue is emptied, and further invocations
 *       will have no affect.
 */
//void css_process_pending_reloads(void);

/*! \brief Load XML documentation. Provided by xmldoc.c 
 *  \retval 1 on error.
 *  \retval 0 on success. 
 */
//int css_xmldoc_load_documentation(void);

/*!
 * \brief Reload genericplc configuration value from codecs.conf
 *
 * Implementation is in main/channel.c
 */
//int css_plc_reload(void);


#ifdef	__cplusplus
}
#endif

#endif	/* _PRIVATE_H */

